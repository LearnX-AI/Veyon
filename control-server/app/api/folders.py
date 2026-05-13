"""Shared folder endpoints - admin side."""

from __future__ import annotations

from fastapi import (
    APIRouter,
    Depends,
    File,
    Form,
    HTTPException,
    Request,
    Response,
    UploadFile,
    status,
)
from fastapi.responses import StreamingResponse
from sqlalchemy import func, select
from sqlalchemy.orm import Session

from app.core.audit import log_action
from app.core.config import Settings, get_settings
from app.core.file_storage import (
    delete_file as delete_stored_file,
    ensure_storage_root,
    ensure_submission_root,
    iter_submission_chunks,
    save_upload_stream,
)
from app.core.security import verify_token
from app.db.database import get_db
from app.models import (
    DistributionStatus,
    FileDistribution,
    FileRecord,
    FolderAssignment,
    FolderStatus,
    Machine,
    SharedFolder,
    Submission,
)
from app.schemas import (
    FolderAssignRequest,
    FolderCreate,
    FolderRead,
    FolderUpdate,
    SubmissionRead,
)


router = APIRouter(prefix="/folders", tags=["folders"])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _folder_or_404(db: Session, folder_id: int) -> SharedFolder:
    folder = db.get(SharedFolder, folder_id)
    if folder is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Folder {folder_id} not found.",
        )
    return folder


def _to_read(db: Session, folder: SharedFolder) -> FolderRead:
    """Attach summary counts to a FolderRead response."""
    machine_count = db.scalar(
        select(func.count()).select_from(FolderAssignment)
        .where(FolderAssignment.folder_id == folder.id)
    ) or 0

    material_count = db.scalar(
        select(func.count(func.distinct(FileDistribution.file_id)))
        .where(FileDistribution.folder_id == folder.id)
    ) or 0

    submission_count = db.scalar(
        select(func.count()).select_from(Submission)
        .where(Submission.folder_id == folder.id)
    ) or 0

    out = FolderRead.model_validate(folder)
    out.machine_count = machine_count
    out.material_count = material_count
    out.submission_count = submission_count
    return out


# ---------------------------------------------------------------------------
# Admin endpoints
# ---------------------------------------------------------------------------

@router.get("", response_model=list[FolderRead])
def list_folders(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[FolderRead]:
    folders = list(db.scalars(
        select(SharedFolder).order_by(SharedFolder.created_at.desc())
    ))
    return [_to_read(db, f) for f in folders]


@router.post("", response_model=FolderRead, status_code=status.HTTP_201_CREATED)
def create_folder(
    payload: FolderCreate,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> FolderRead:
    """Create a new shared folder and optionally assign machines."""
    # Uniqueness check (DB-level UNIQUE handles races; this is for the nice error message)
    existing = db.scalar(select(SharedFolder).where(SharedFolder.name == payload.name))
    if existing is not None:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"A folder named '{payload.name}' already exists.",
        )

    folder = SharedFolder(
        name=payload.name,
        description=payload.description,
        deadline=payload.deadline,
        status=FolderStatus.OPEN,
    )
    db.add(folder)
    db.flush()       # get an id without committing yet

    # Validate machine IDs and assign them
    if payload.machine_ids:
        requested = set(payload.machine_ids)
        valid = list(db.scalars(
            select(Machine).where(Machine.id.in_(requested))
        ))
        valid_ids = {m.id for m in valid}
        missing = requested - valid_ids
        if missing:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Unknown machine id(s): {sorted(missing)}",
            )
        for mid in valid_ids:
            db.add(FolderAssignment(folder_id=folder.id, machine_id=mid))

    log_action(
        db,
        action="folder.create",
        target=folder.name,
        details=f"{len(payload.machine_ids)} machine(s)",
        request=request,
    )
    db.commit()
    db.refresh(folder)
    return _to_read(db, folder)


# ---------------------------------------------------------------------------
# Agent endpoints
# ---------------------------------------------------------------------------

from fastapi import Header                          # noqa: E402
from app.core.file_storage import save_submission_stream  # noqa: E402
from app.schemas import AgentFolderMaterial, AgentFolderState  # noqa: E402


def _resolve_machine(db: Session, hostname: str | None) -> Machine:
    if not hostname:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing X-Veyon-Hostname header",
        )
    machine = db.scalar(select(Machine).where(Machine.hostname == hostname))
    if machine is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Unknown machine '{hostname}'. Call /machines/register first.",
        )
    return machine


@router.get("/agent-sync", response_model=list[AgentFolderState])
def agent_folder_sync(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
    x_veyon_hostname: str | None = Header(default=None),
) -> list[AgentFolderState]:
    """
    Return this machine's view of all assigned folders + their materials.
    Used by the agent on every sync tick.
    """
    machine = _resolve_machine(db, x_veyon_hostname)

    # All folders this machine is assigned to
    folders = list(db.scalars(
        select(SharedFolder)
        .join(FolderAssignment, FolderAssignment.folder_id == SharedFolder.id)
        .where(FolderAssignment.machine_id == machine.id)
        .order_by(SharedFolder.created_at.desc())
    ))

    out: list[AgentFolderState] = []
    for folder in folders:
        # Get this folder's materials for this machine
        stmt = (
            select(FileDistribution, FileRecord)
            .join(FileRecord, FileDistribution.file_id == FileRecord.id)
            .where(
                FileDistribution.folder_id == folder.id,
                FileDistribution.machine_id == machine.id,
            )
            .order_by(FileDistribution.created_at)
        )
        materials = [
            AgentFolderMaterial(
                distribution_id=dist.id,
                file_id=rec.id,
                filename=rec.filename,
                sha256=rec.sha256,
                size_bytes=rec.size_bytes,
            )
            for dist, rec in db.execute(stmt).all()
        ]

        out.append(AgentFolderState(
            folder_id=folder.id,
            name=folder.name,
            status=folder.status,
            deadline=folder.deadline,
            materials=materials,
        ))

    return out


@router.post(
    "/{folder_id}/submit",
    response_model=SubmissionRead,
    status_code=status.HTTP_201_CREATED,
)
async def agent_submit_file(
    folder_id: int,
    request: Request,
    file: UploadFile = File(...),
    note: str | None = Form(default=None),
    db: Session = Depends(get_db),
    settings: Settings = Depends(get_settings),
    _: str = Depends(verify_token),
    x_veyon_hostname: str | None = Header(default=None),
) -> Submission:
    """
    Agent posts a student submission into a folder.

    Validation:
      - Folder must exist and be OPEN
      - This machine must be assigned to the folder
    """
    machine = _resolve_machine(db, x_veyon_hostname)
    folder = _folder_or_404(db, folder_id)

    if folder.status == FolderStatus.CLOSED:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Folder is closed; submissions are no longer accepted.",
        )

    assigned = db.scalar(
        select(FolderAssignment).where(
            FolderAssignment.folder_id == folder_id,
            FolderAssignment.machine_id == machine.id,
        )
    )
    if assigned is None:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="This machine is not assigned to that folder.",
        )

    if not file.filename:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing filename",
        )

    ensure_submission_root()

    async def _streamer():
        while True:
            chunk = await file.read(1024 * 1024)
            if not chunk:
                break
            yield chunk

    try:
        storage_id, safe_filename, sha256, size = await save_submission_stream(
            _streamer(),
            file.filename,
            max_bytes=settings.file_max_size_bytes,
        )
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=str(exc),
        )

    record = Submission(
        folder_id=folder_id,
        machine_id=machine.id,
        storage_id=storage_id,
        filename=safe_filename,
        sha256=sha256,
        size_bytes=size,
        content_type=file.content_type,
        note=note,
    )
    db.add(record)
    log_action(
        db,
        action="folder.submission",
        target=folder.name,
        details=f"{safe_filename} from {machine.hostname} ({size} B)",
        request=request,
    )
    db.commit()
    db.refresh(record)
    return record


@router.get("/{folder_id}", response_model=FolderRead)
def get_folder(
    folder_id: int,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> FolderRead:
    return _to_read(db, _folder_or_404(db, folder_id))


@router.patch("/{folder_id}", response_model=FolderRead)
def update_folder(
    folder_id: int,
    payload: FolderUpdate,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> FolderRead:
    folder = _folder_or_404(db, folder_id)
    changes: list[str] = []

    if payload.description is not None:
        folder.description = payload.description
        changes.append("description")
    if payload.deadline is not None:
        folder.deadline = payload.deadline
        changes.append("deadline")
    if payload.status is not None and payload.status != folder.status:
        folder.status = payload.status
        changes.append(f"status={payload.status.value}")

    if changes:
        log_action(
            db,
            action="folder.update",
            target=folder.name,
            details=", ".join(changes),
            request=request,
        )
        db.commit()
        db.refresh(folder)
    return _to_read(db, folder)


@router.delete("/{folder_id}", status_code=status.HTTP_204_NO_CONTENT, response_class=Response)
def delete_folder(
    folder_id: int,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Response:
    folder = _folder_or_404(db, folder_id)
    name = folder.name

    # Collect submissions to clean from disk after DB cascade
    submissions = list(db.scalars(
        select(Submission).where(Submission.folder_id == folder_id)
    ))

    db.delete(folder)
    log_action(db, action="folder.delete", target=name, request=request)
    db.commit()

    # Remove submission bytes from disk
    from app.core.file_storage import delete_submission
    for s in submissions:
        delete_submission(s.storage_id)

    return Response(status_code=status.HTTP_204_NO_CONTENT)


# ---- Machine assignment ----

@router.post("/{folder_id}/assign", status_code=status.HTTP_201_CREATED, response_class=Response)
def assign_machines(
    folder_id: int,
    payload: FolderAssignRequest,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Response:
    folder = _folder_or_404(db, folder_id)

    requested = set(payload.machine_ids)
    valid = list(db.scalars(select(Machine).where(Machine.id.in_(requested))))
    valid_ids = {m.id for m in valid}
    missing = requested - valid_ids
    if missing:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Unknown machine id(s): {sorted(missing)}",
        )

    # Skip already-assigned machines
    already = {a.machine_id for a in db.scalars(
        select(FolderAssignment).where(
            FolderAssignment.folder_id == folder_id,
            FolderAssignment.machine_id.in_(valid_ids),
        )
    )}
    to_add = valid_ids - already
    for mid in to_add:
        db.add(FolderAssignment(folder_id=folder_id, machine_id=mid))

    if to_add:
        log_action(
            db,
            action="folder.assign",
            target=folder.name,
            details=f"+{len(to_add)} machine(s)",
            request=request,
        )
        db.commit()

    return Response(status_code=status.HTTP_201_CREATED)


@router.delete("/{folder_id}/assign/{machine_id}",
               status_code=status.HTTP_204_NO_CONTENT, response_class=Response)
def unassign_machine(
    folder_id: int,
    machine_id: int,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Response:
    folder = _folder_or_404(db, folder_id)
    assignment = db.scalar(
        select(FolderAssignment).where(
            FolderAssignment.folder_id == folder_id,
            FolderAssignment.machine_id == machine_id,
        )
    )
    if assignment is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Machine {machine_id} is not assigned to this folder.",
        )
    db.delete(assignment)
    log_action(
        db,
        action="folder.unassign",
        target=folder.name,
        details=f"machine_id={machine_id}",
        request=request,
    )
    db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)


# ---- Materials (teacher uploads, auto-distributes to assigned machines) ----

@router.post("/{folder_id}/materials",
             response_model=list[int], status_code=status.HTTP_201_CREATED)
async def upload_material(
    folder_id: int,
    request: Request,
    file: UploadFile = File(...),
    note: str | None = Form(default=None),
    db: Session = Depends(get_db),
    settings: Settings = Depends(get_settings),
    _: str = Depends(verify_token),
) -> list[int]:
    """
    Upload a material file. It's stored once on the server then automatically
    queued for delivery to every machine currently assigned to the folder.
    Returns the list of distribution IDs created.
    """
    folder = _folder_or_404(db, folder_id)
    if folder.status == FolderStatus.CLOSED:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Folder is closed; cannot add new materials.",
        )

    ensure_storage_root()

    if not file.filename:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing filename",
        )

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

    from datetime import UTC, datetime, timedelta
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
    db.flush()

    # Auto-distribute to every assigned machine
    assignments = list(db.scalars(
        select(FolderAssignment).where(FolderAssignment.folder_id == folder_id)
    ))
    dist_ids: list[int] = []
    for a in assignments:
        d = FileDistribution(
            file_id=record.id,
            machine_id=a.machine_id,
            folder_id=folder_id,
            status=DistributionStatus.QUEUED,
        )
        db.add(d)
        db.flush()
        dist_ids.append(d.id)

    log_action(
        db,
        action="folder.material_upload",
        target=folder.name,
        details=f"{safe_filename} -> {len(dist_ids)} machine(s)",
        request=request,
    )
    db.commit()
    return dist_ids


# ---- Submissions (read-only from admin side) ----

@router.get("/{folder_id}/submissions", response_model=list[SubmissionRead])
def list_submissions(
    folder_id: int,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[Submission]:
    _folder_or_404(db, folder_id)
    return list(db.scalars(
        select(Submission)
        .where(Submission.folder_id == folder_id)
        .order_by(Submission.submitted_at.desc())
    ))


@router.get("/{folder_id}/submissions/{submission_id}/download")
async def download_submission(
    folder_id: int,
    submission_id: int,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> StreamingResponse:
    """Teacher downloads a student submission."""
    sub = db.get(Submission, submission_id)
    if sub is None or sub.folder_id != folder_id:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Submission not found for this folder.",
        )

    headers = {
        "Content-Disposition": f'attachment; filename="{sub.filename}"',
        "X-Veyon-File-Sha256": sub.sha256,
        "X-Veyon-File-Size":   str(sub.size_bytes),
    }
    return StreamingResponse(
        iter_submission_chunks(sub.storage_id, sub.filename),
        media_type=sub.content_type or "application/octet-stream",
        headers=headers,
    )
