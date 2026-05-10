"""Machine registry and per-machine control endpoints."""

from datetime import UTC, datetime

from fastapi import APIRouter, Depends, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.core.audit import log_action
from app.core.security import verify_token
from app.db.database import get_db
from app.models import Machine
from app.schemas import MachineHeartbeat, MachineRead, MachineRegister


router = APIRouter(prefix="/machines", tags=["machines"])


@router.get("", response_model=list[MachineRead])
def list_machines(
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> list[Machine]:
    """List all registered student PCs."""
    stmt = select(Machine).order_by(Machine.hostname)
    return list(db.scalars(stmt))


@router.post(
    "/register",
    response_model=MachineRead,
    status_code=status.HTTP_201_CREATED,
)
def register_machine(
    payload: MachineRegister,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Machine:
    """Register a new student PC. Idempotent on hostname."""
    # If hostname already exists, update its IP/label and treat as re-registration.
    existing = db.scalar(
        select(Machine).where(Machine.hostname == payload.hostname)
    )
    if existing is not None:
        if payload.ip_address is not None:
            existing.ip_address = str(payload.ip_address)
        if payload.label is not None:
            existing.label = payload.label
        existing.last_seen_at = datetime.now(UTC)
        log_action(
            db,
            action="machine.reregister",
            target=payload.hostname,
            request=request,
        )
        db.commit()
        db.refresh(existing)
        return existing

    machine = Machine(
        hostname=payload.hostname,
        ip_address=str(payload.ip_address) if payload.ip_address else None,
        label=payload.label,
        last_seen_at=datetime.now(UTC),
    )
    db.add(machine)
    log_action(
        db,
        action="machine.register",
        target=payload.hostname,
        request=request,
    )
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Machine '{payload.hostname}' already exists.",
        )
    db.refresh(machine)
    return machine


@router.post("/heartbeat", response_model=MachineRead)
def heartbeat(
    payload: MachineHeartbeat,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Machine:
    """
    Sync agent check-in. The agent posts its current blocklist version;
    if the server's version is higher, the agent should pull the new list.
    """
    machine = db.scalar(
        select(Machine).where(Machine.hostname == payload.hostname)
    )
    if machine is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Unknown machine '{payload.hostname}'. Call /register first.",
        )

    machine.last_seen_at = datetime.now(UTC)
    db.commit()
    db.refresh(machine)
    return machine


@router.post("/{machine_id}/focus-mode", response_model=MachineRead)
def set_focus_mode(
    machine_id: int,
    enabled: bool,
    request: Request,
    db: Session = Depends(get_db),
    _: str = Depends(verify_token),
) -> Machine:
    """Toggle Focus Mode on or off for a single machine."""
    machine = db.get(Machine, machine_id)
    if machine is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Machine {machine_id} not found.",
        )

    machine.focus_mode_active = enabled
    machine.blocklist_version += 1   # force the sync agent to re-pull state
    log_action(
        db,
        action="machine.focus_on" if enabled else "machine.focus_off",
        target=machine.hostname,
        request=request,
    )
    db.commit()
    db.refresh(machine)
    return machine
