"""Request/response schemas for session endpoints."""

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field, field_validator

from app.models.session import SessionMode, SessionStatus, TimeoutAction
from app.models.session_event import SessionEventType


# ---------------------------------------------------------------------------
# Admin schemas
# ---------------------------------------------------------------------------

class SessionCreate(BaseModel):
    """Body for POST /sessions."""
    name: str = Field(..., min_length=1, max_length=255)
    description: str | None = Field(default=None, max_length=2000)
    mode: SessionMode = SessionMode.LAB
    duration_minutes: int = Field(..., ge=1, le=1440)   # 1 min .. 24 h
    warning_minutes: list[int] = Field(default_factory=lambda: [10, 1])
    timeout_action: TimeoutAction = TimeoutAction.LOCK_SCREEN
    machine_ids: list[int] = Field(default_factory=list)

    @field_validator("warning_minutes")
    @classmethod
    def _validate_warnings(cls, v: list[int]) -> list[int]:
        # Sort desc so [15, 5, 1] is consistent ordering
        if any(m <= 0 for m in v):
            raise ValueError("warning_minutes entries must be positive")
        return sorted(set(v), reverse=True)


class SessionUpdate(BaseModel):
    """Body for PATCH /sessions/{id}. All fields optional, only valid pre-start."""
    name: str | None = Field(default=None, min_length=1, max_length=255)
    description: str | None = Field(default=None, max_length=2000)
    duration_minutes: int | None = Field(default=None, ge=1, le=1440)
    warning_minutes: list[int] | None = None
    timeout_action: TimeoutAction | None = None


class SessionRead(BaseModel):
    """Returned by GET /sessions."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    name: str
    description: str | None
    mode: SessionMode
    duration_minutes: int
    warning_minutes: list[int]
    timeout_action: TimeoutAction
    status: SessionStatus
    started_at: datetime | None
    ends_at: datetime | None
    paused_at: datetime | None
    created_at: datetime
    created_by: str | None

    # Summary counts filled by the endpoint
    machine_count: int = 0


class SessionExtendRequest(BaseModel):
    """Body for POST /sessions/{id}/extend."""
    minutes: int = Field(..., ge=1, le=1440)


class SessionEventRead(BaseModel):
    """Returned by GET /sessions/{id}/events."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    session_id: int
    machine_id: int | None
    event_type: SessionEventType
    details: str | None
    occurred_at: datetime


# ---------------------------------------------------------------------------
# Agent schemas
# ---------------------------------------------------------------------------

class AgentActiveSession(BaseModel):
    """Active session this machine should enforce. Returned by GET /sessions/agent-active."""
    session_id: int
    name: str
    status: SessionStatus
    mode: SessionMode
    ends_at: datetime
    warning_minutes: list[int]
    timeout_action: TimeoutAction


class AgentSessionEventReport(BaseModel):
    """Agent tells us it did something. POST /sessions/{id}/agent-event."""
    event_type: SessionEventType
    details: str | None = Field(default=None, max_length=500)
