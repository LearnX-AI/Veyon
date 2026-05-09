"""Blocklist management endpoints."""

from fastapi import APIRouter, Depends
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.security import verify_token
from app.db.database import get_db
from app.models import BlocklistEntry
from app.schemas import BlocklistEntryRead


router = APIRouter(prefix="/blocklist", tags=["blocklist"])


@router.get("", response_model=list[BlocklistEntryRead])
def list_blocklist(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[BlocklistEntry]:
    """List all blocked domains."""
    stmt = select(BlocklistEntry).order_by(BlocklistEntry.domain)
    return list(db.scalars(stmt))
