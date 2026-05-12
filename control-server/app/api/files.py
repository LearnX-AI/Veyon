"""File distribution endpoints (admin side)."""

from __future__ import annotations

from datetime import UTC, datetime, timedelta

from fastapi import APIRouter, Depends, File, Form, HTTPException, Request, Response, UploadFile, status
from fastapi.responses import StreamingResponse
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.audit import log_action
from app.core.config import Settings, get_settings
from app.core.file_storage import (
    ensure_storage_root,
    delete_file as delete_stored_file,
    iter_file_chunks,
    save_upload_stream,
)
from app.core.security import verify_token
from app.db.database import get_db
from app.models import (
    DistributionStatus,
    FileDistribution,
    FileRecord,
    Machine,
)
from app.schemas import (
    DistributeRequest,
    FileDistributionRead,
    FileRead,
)


router = APIRouter(prefix="/files", tags=["files"])


# ---------------------------------------------------------------------------
# Admin endpoints (Bearer-token auth)
# ---------------------------------------------------------------------------

@router.get("", response_model=list[FileRead])
def list_files(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[FileRecord]:
    """List all stored files, newest first."""
    stmt = select(FileRecord).order_by(FileRecord.uploaded_at.desc())
    return list(db.scalars(stmt))


@router.post(
    "/upload",
    response_model=FileRead,
    status_code=status.HTTP_201_CREATED,
)
async def upload_file(
    request: Request,
    file: UploadFile = File(...),
    note: str | None = Form(default=None),
    db: Session = Depends(get_db),
    settings: Settings = Depends(get_settings),
    _: str = Depends(verify_token),
) -> FileRecord:
    """
    Upload a file. Streams to disk in chunks - never loads the full
    file into memory, so 1GB uploads are fine.
    """
    ensure_storage_root()

    if not file.filename:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing filename",
        )

    # Read in chunks from the multipart stream
    async def _streamer():
        while True:
            chunk = await file.read(1024 * 1024)
            if not chunk:
                break
            yield chunk

    try:
        storage_id, safe_filename, sha256, size = await save_upload_stream(
            _streamer(),
            file.filename,
            max_bytes=settings.file_max_size_bytes,
        )
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=str(exc),
        )

    expires_at = datetime.now(UTC) + timedelta(days=settings.file_retention_days)

    record = FileRecord(
        storage_id=storage_id,
        filename=safe_filename,
        sha256=sha256,
        size_bytes=size,
        content_type=file.content_type,
        note=note,
        expires_at=expires_at,
    )
    db.add(record)
    log_action(
        db,
        action="file.upload",
        target=safe_filename,
        details=f"{size} bytes, sha256={sha256[:16]}...",
        request=request,
    )
    db.commit()
    db.refresh(record)
    return record


@router.delete("/{file_id}", status_code=status.HTTP_204_NO_CONTENT, response_class=Response)
def delete_file(
    file_id: int,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Response:
    """Delete a file (server copy) and all its pending/done distributions."""
    record = db.get(FileRecord, file_id)
    if record is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"File {file_id} not found.",
        )

    storage_id = record.storage_id
    filename = record.filename
    db.delete(record)        # cascades to file_distributions
    log_action(
        db,
        action="file.delete",
        target=filename,
        request=request,
    )
    db.commit()

    # Remove bytes from disk after the DB row is gone
    delete_stored_file(storage_id)
    return Response(status_code=status.HTTP_204_NO_CONTENT)


@router.post(
    "/{file_id}/distribute",
    response_model=list[FileDistributionRead],
    status_code=status.HTTP_201_CREATED,
)
def distribute_file(
    file_id: int,
    payload: DistributeRequest,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[FileDistribution]:
    """Queue a file for delivery to one or more machines."""
    record = db.get(FileRecord, file_id)
    if record is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"File {file_id} not found.",
        )

    # Validate every target machine exists
    requested = set(payload.machine_ids)
    valid_machines = list(db.scalars(
        select(Machine).where(Machine.id.in_(requested))
    ))
    found_ids = {m.id for m in valid_machines}
    missing = requested - found_ids
    if missing:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Unknown machine id(s): {sorted(missing)}",
        )

    # Reuse existing rows if this file was already queued for the same machine,
    # but reset their state so the agent re-downloads.
    existing = {
        d.machine_id: d for d in db.scalars(
            select(FileDistribution).where(
                FileDistribution.file_id == file_id,
                FileDistribution.machine_id.in_(found_ids),
            )
        )
    }

    distributions: list[FileDistribution] = []
    for machine_id in found_ids:
        if machine_id in existing:
            d = existing[machine_id]
            d.status = DistributionStatus.QUEUED
            d.bytes_received = 0
            d.error_message = None
            d.completed_at = None
        else:
            d = FileDistribution(
                file_id=file_id,
                machine_id=machine_id,
                status=DistributionStatus.QUEUED,
            )
            db.add(d)
        distributions.append(d)

    log_action(
        db,
        action="file.distribute",
        target=record.filename,
        details=f"to {len(distributions)} machine(s)",
        request=request,
    )
    db.commit()
    for d in distributions:
        db.refresh(d)
    return distributions


@router.get("/{file_id}/distributions", response_model=list[FileDistributionRead])
def list_distributions(
    file_id: int,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[FileDistribution]:
    """Per-machine delivery status for a file (for the progress UI)."""
    if db.get(FileRecord, file_id) is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"File {file_id} not found.",
        )
    stmt = (
        select(FileDistribution)
        .where(FileDistribution.file_id == file_id)
        .order_by(FileDistribution.machine_id)
    )
    return list(db.scalars(stmt))
