"""Request/response schemas for machine endpoints."""

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field, IPvAnyAddress


class MachineRegister(BaseModel):
    """Payload for a student PC to register itself with the server."""
    hostname: str = Field(..., min_length=1, max_length=255)
    ip_address: IPvAnyAddress | None = None
    label: str | None = Field(default=None, max_length=100)


class MachineRead(BaseModel):
    """A machine record returned to the client."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    hostname: str
    ip_address: str | None
    label: str | None
    focus_mode_active: bool
    blocklist_version: int
    last_seen_at: datetime | None
    registered_at: datetime


class MachineHeartbeat(BaseModel):
    """Sent by the sync agent on every poll."""
    hostname: str = Field(..., min_length=1, max_length=255)
    current_blocklist_version: int = Field(..., ge=0)
