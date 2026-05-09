"""Blocklist management endpoints."""

from fastapi import APIRouter, Depends, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.core.audit import log_action
from app.core.security import verify_token
from app.db.database import get_db
from app.models import BlocklistEntry, Machine
from app.schemas import BlocklistEntryCreate, BlocklistEntryRead


router = APIRouter(prefix="/blocklist", tags=["blocklist"])


def _bump_machine_versions(db: Session) -> None:
    """
    Increment blocklist_version on every registered machine.
    Sync agents poll this number and fetch the new list when it changes.
    """
    for m in db.scalars(select(Machine)):
        m.blocklist_version += 1


@router.get("", response_model=list[BlocklistEntryRead])
def list_blocklist(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[BlocklistEntry]:
    """List all blocked domains, alphabetically."""
    stmt = select(BlocklistEntry).order_by(BlocklistEntry.domain)
    return list(db.scalars(stmt))


@router.post(
    "",
    response_model=BlocklistEntryRead,
    status_code=status.HTTP_201_CREATED,
)
def add_blocklist_entry(
    payload: BlocklistEntryCreate,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> BlocklistEntry:
    """Add a domain to the blocklist."""
    entry = BlocklistEntry(domain=payload.domain, note=payload.note)
    db.add(entry)
    _bump_machine_versions(db)
    log_action(
        db,
        action="blocklist.add",
        target=payload.domain,
        details=payload.note,
        request=request,
    )
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Domain '{payload.domain}' is already in the blocklist.",
        )
    db.refresh(entry)
    return entry


@router.delete("/{entry_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_blocklist_entry(
    entry_id: int,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> None:
    """Remove a domain from the blocklist."""
    entry = db.get(BlocklistEntry, entry_id)
    if entry is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Blocklist entry {entry_id} not found.",
        )

    domain = entry.domain
    db.delete(entry)
    _bump_machine_versions(db)
    log_action(
        db,
        action="blocklist.remove",
        target=domain,
        request=request,
    )
    db.commit()
