"""
Audit log helper.

Any state-changing endpoint should call log_action() to record
what happened, by whom, when. This implements the
"Maintain logs of restricted access actions" requirement.
"""

from fastapi import Request
from sqlalchemy.orm import Session

from app.models import AdminAction


def log_action(
    db: Session,
    *,
    action: str,
    target: str | None = None,
    details: str | None = None,
    request: Request | None = None,
) -> AdminAction:
    """
    Record an admin action to the audit log.

    Args:
        db: active DB session (caller is responsible for commit)
        action: short identifier, e.g. "blocklist.add", "machine.focus_on"
        target: what was acted on, e.g. "facebook.com" or "PC-LAB-12"
        details: free-form extra context (JSON-encode if structured)
        request: FastAPI Request - extracts client IP automatically

    Returns:
        The created AdminAction (already added to the session, not yet committed).
    """
    actor_ip = None
    if request is not None and request.client is not None:
        actor_ip = request.client.host

    entry = AdminAction(
        action=action,
        target=target,
        details=details,
        actor_ip=actor_ip,
    )
    db.add(entry)
    return entry
