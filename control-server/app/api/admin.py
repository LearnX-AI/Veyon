"""Admin endpoints (audit log viewer)."""

from fastapi import APIRouter, Depends, Query
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.security import verify_token
from app.db.database import get_db
from app.models import AdminAction
from app.schemas import AdminActionRead


router = APIRouter(prefix="/admin", tags=["admin"])


@router.get("/log", response_model=list[AdminActionRead])
def list_admin_log(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
    action: str | None = Query(default=None, description="Filter by action name (e.g. 'blocklist.add')"),
    limit: int = Query(default=100, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
) -> list[AdminAction]:
    """List recent audit log entries, newest first."""
    stmt = select(AdminAction).order_by(AdminAction.occurred_at.desc())
    if action is not None:
        stmt = stmt.where(AdminAction.action == action)
    stmt = stmt.limit(limit).offset(offset)
    return list(db.scalars(stmt))
