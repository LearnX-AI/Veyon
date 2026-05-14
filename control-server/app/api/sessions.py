"""Time-limited session endpoints."""

from __future__ import annotations

from datetime import UTC, datetime, timedelta

from fastapi import APIRouter, Depends, Header, HTTPException, Request, Response, status
from sqlalchemy import func, select
from sqlalchemy.orm import Session as DbSession

from app.core.audit import log_action
from app.core.security import verify_token
from app.db.database import get_db
from app.models import (
    Machine,
    Session,
    SessionEvent,
    SessionEventType,
    SessionMachine,
    SessionStatus,
)
from app.schemas import (
    AgentActiveSession,
    AgentSessionEventReport,
    SessionCreate,
    SessionEventRead,
    SessionExtendRequest,
    SessionRead,
    SessionUpdate,
)


router = APIRouter(prefix="/sessions", tags=["sessions"])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _session_or_404(db: DbSession, session_id: int) -> Session:
    s = db.get(Session, session_id)
    if s is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Session {session_id} not found.",
        )
    return s


def _to_read(db: DbSession, s: Session) -> SessionRead:
    machine_count = db.scalar(
        select(func.count()).select_from(SessionMachine)
        .where(SessionMachine.session_id == s.id)
    ) or 0
    out = SessionRead.model_validate(s)
    out.machine_count = machine_count
    return out


def _record_event(
    db: DbSession,
    session_id: int,
    event_type: SessionEventType,
    *,
    machine_id: int | None = None,
    details: str | None = None,
) -> None:
    db.add(SessionEvent(
        session_id=session_id,
        machine_id=machine_id,
        event_type=event_type,
        details=details,
    ))


def _resolve_machine(db: DbSession, hostname: str | None) -> Machine:
    if not hostname:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Missing X-Veyon-Hostname header",
        )
    machine = db.scalar(select(Machine).where(Machine.hostname == hostname))
    if machine is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Unknown machine '{hostname}'.",
        )
    return machine


# ---------------------------------------------------------------------------
# Agent endpoints (MUST come before /{session_id} catch-all)
# ---------------------------------------------------------------------------

@router.get("/agent-active", response_model=AgentActiveSession | None)
def agent_active_session(
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
    x_veyon_hostname: str | None = Header(default=None),
) -> AgentActiveSession | None:
    """
    Return the currently RUNNING session this machine is assigned to,
    if any. Used by the agent on every sync tick.
    Returns null when there's no active session.
    """
    machine = _resolve_machine(db, x_veyon_hostname)

    s = db.scalar(
        select(Session)
        .join(SessionMachine, SessionMachine.session_id == Session.id)
        .where(
            SessionMachine.machine_id == machine.id,
            Session.status == SessionStatus.RUNNING,
        )
        .order_by(Session.started_at.desc())
        .limit(1)
    )
    if s is None or s.ends_at is None:
        return None

    return AgentActiveSession(
        session_id=s.id,
        name=s.name,
        status=s.status,
        mode=s.mode,
        ends_at=s.ends_at,
        warning_minutes=s.warning_minutes,
        timeout_action=s.timeout_action,
    )


# ---------------------------------------------------------------------------
# Admin endpoints
# ---------------------------------------------------------------------------

@router.get("", response_model=list[SessionRead])
def list_sessions(
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[SessionRead]:
    sessions = list(db.scalars(
        select(Session).order_by(Session.created_at.desc())
    ))
    return [_to_read(db, s) for s in sessions]


@router.post("", response_model=SessionRead, status_code=status.HTTP_201_CREATED)
def create_session(
    payload: SessionCreate,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    """Create a session in SCHEDULED state. Not yet running."""
    s = Session(
        name=payload.name,
        description=payload.description,
        mode=payload.mode,
        duration_minutes=payload.duration_minutes,
        warning_minutes=payload.warning_minutes,
        timeout_action=payload.timeout_action,
        status=SessionStatus.SCHEDULED,
    )
    db.add(s)
    db.flush()

    # Assign machines
    if payload.machine_ids:
        requested = set(payload.machine_ids)
        valid = list(db.scalars(select(Machine).where(Machine.id.in_(requested))))
        valid_ids = {m.id for m in valid}
        missing = requested - valid_ids
        if missing:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Unknown machine id(s): {sorted(missing)}",
            )
        for mid in valid_ids:
            db.add(SessionMachine(session_id=s.id, machine_id=mid))

    log_action(
        db, action="session.create", target=s.name,
        details=f"{payload.duration_minutes} min, {len(payload.machine_ids)} machines",
        request=request,
    )
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


@router.get("/{session_id}", response_model=SessionRead)
def get_session(
    session_id: int,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    return _to_read(db, _session_or_404(db, session_id))


@router.patch("/{session_id}", response_model=SessionRead)
def update_session(
    session_id: int,
    payload: SessionUpdate,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    """Edit a session. Only allowed while SCHEDULED."""
    s = _session_or_404(db, session_id)
    if s.status != SessionStatus.SCHEDULED:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Cannot edit session in state {s.status.value}.",
        )

    changes: list[str] = []
    if payload.name is not None:
        s.name = payload.name; changes.append("name")
    if payload.description is not None:
        s.description = payload.description; changes.append("description")
    if payload.duration_minutes is not None:
        s.duration_minutes = payload.duration_minutes; changes.append("duration")
    if payload.warning_minutes is not None:
        s.warning_minutes = sorted(set(payload.warning_minutes), reverse=True)
        changes.append("warnings")
    if payload.timeout_action is not None:
        s.timeout_action = payload.timeout_action; changes.append("action")

    if changes:
        log_action(
            db, action="session.update", target=s.name,
            details=", ".join(changes), request=request,
        )
        db.commit()
        db.refresh(s)
    return _to_read(db, s)


@router.delete("/{session_id}", status_code=status.HTTP_204_NO_CONTENT, response_class=Response)
def delete_session(
    session_id: int,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> Response:
    s = _session_or_404(db, session_id)
    name = s.name
    db.delete(s)        # cascades to session_machines and session_events
    log_action(db, action="session.delete", target=name, request=request)
    db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)


# ---- Lifecycle controls ----

@router.post("/{session_id}/start", response_model=SessionRead)
def start_session(
    session_id: int,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    """Transition SCHEDULED -> RUNNING. Sets started_at and ends_at."""
    s = _session_or_404(db, session_id)
    if s.status != SessionStatus.SCHEDULED:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Session is {s.status.value}; can only start a SCHEDULED session.",
        )

    now = datetime.now(UTC)
    s.status = SessionStatus.RUNNING
    s.started_at = now
    s.ends_at = now + timedelta(minutes=s.duration_minutes)

    _record_event(db, session_id, SessionEventType.STARTED,
                  details=f"duration={s.duration_minutes}min, ends_at={s.ends_at.isoformat()}")
    log_action(db, action="session.start", target=s.name, request=request)
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


@router.post("/{session_id}/pause", response_model=SessionRead)
def pause_session(
    session_id: int,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    s = _session_or_404(db, session_id)
    if s.status != SessionStatus.RUNNING:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Cannot pause session in state {s.status.value}.",
        )
    s.status = SessionStatus.PAUSED
    s.paused_at = datetime.now(UTC)
    _record_event(db, session_id, SessionEventType.PAUSED)
    log_action(db, action="session.pause", target=s.name, request=request)
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


@router.post("/{session_id}/resume", response_model=SessionRead)
def resume_session(
    session_id: int,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    """
    Resume a PAUSED session. Shifts ends_at forward by the pause duration,
    so the student gets back exactly the time they had remaining.
    """
    s = _session_or_404(db, session_id)
    if s.status != SessionStatus.PAUSED:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Cannot resume session in state {s.status.value}.",
        )

    now = datetime.now(UTC)
    if s.paused_at is not None and s.ends_at is not None:
        # Make paused_at offset-aware if needed (SQLite stores naive)
        paused = s.paused_at if s.paused_at.tzinfo else s.paused_at.replace(tzinfo=UTC)
        pause_duration = now - paused
        s.ends_at = (s.ends_at if s.ends_at.tzinfo else s.ends_at.replace(tzinfo=UTC)) + pause_duration

    s.paused_at = None
    s.status = SessionStatus.RUNNING
    _record_event(db, session_id, SessionEventType.RESUMED)
    log_action(db, action="session.resume", target=s.name, request=request)
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


@router.post("/{session_id}/extend", response_model=SessionRead)
def extend_session(
    session_id: int,
    payload: SessionExtendRequest,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    s = _session_or_404(db, session_id)
    if s.status not in (SessionStatus.RUNNING, SessionStatus.PAUSED):
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Cannot extend session in state {s.status.value}.",
        )
    if s.ends_at is None:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Session has no end time set.",
        )

    ends = s.ends_at if s.ends_at.tzinfo else s.ends_at.replace(tzinfo=UTC)
    s.ends_at = ends + timedelta(minutes=payload.minutes)
    _record_event(db, session_id, SessionEventType.EXTENDED,
                  details=f"+{payload.minutes} min, new ends_at={s.ends_at.isoformat()}")
    log_action(db, action="session.extend", target=s.name,
               details=f"+{payload.minutes} min", request=request)
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


@router.post("/{session_id}/cancel", response_model=SessionRead)
def cancel_session(
    session_id: int,
    request: Request,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> SessionRead:
    """End the session early without firing the timeout action."""
    s = _session_or_404(db, session_id)
    if s.status not in (SessionStatus.RUNNING, SessionStatus.PAUSED):
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Cannot cancel session in state {s.status.value}.",
        )
    s.status = SessionStatus.CANCELLED
    _record_event(db, session_id, SessionEventType.CANCELLED)
    log_action(db, action="session.cancel", target=s.name, request=request)
    db.commit()
    db.refresh(s)
    return _to_read(db, s)


# ---- Events / history ----

@router.get("/{session_id}/events", response_model=list[SessionEventRead])
def list_events(
    session_id: int,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[SessionEvent]:
    _session_or_404(db, session_id)
    return list(db.scalars(
        select(SessionEvent)
        .where(SessionEvent.session_id == session_id)
        .order_by(SessionEvent.occurred_at)
    ))


# ---- Agent event reporting ----

@router.post("/{session_id}/agent-event", status_code=status.HTTP_204_NO_CONTENT, response_class=Response)
def agent_report_event(
    session_id: int,
    payload: AgentSessionEventReport,
    db: DbSession = Depends(get_db),
    _: str = Depends(verify_token),
    x_veyon_hostname: str | None = Header(default=None),
) -> Response:
    """
    Agent tells us it did something (showed a warning, fired the action).
    Records an event and, if the agent reports COMPLETED, flips the
    session status.
    """
    machine = _resolve_machine(db, x_veyon_hostname)
    s = _session_or_404(db, session_id)

    _record_event(
        db, session_id, payload.event_type,
        machine_id=machine.id,
        details=payload.details,
    )

    # If every assigned machine has reported ACTION_FIRED, flip to COMPLETED.
    # For simplicity: any ACTION_FIRED report transitions a RUNNING session
    # to COMPLETED. (Practically all machines fire within seconds of each
    # other since ends_at is wall-clock-shared.)
    if payload.event_type == SessionEventType.ACTION_FIRED and s.status == SessionStatus.RUNNING:
        s.status = SessionStatus.COMPLETED
        _record_event(db, session_id, SessionEventType.COMPLETED)

    db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)
