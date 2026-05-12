"""Request/response schemas for file distribution endpoints."""

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field

from app.models.file_distribution import DistributionStatus


class FileRead(BaseModel):
    """A stored file returned to the client."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    storage_id: str
    filename: str
    sha256: str
    size_bytes: int
    content_type: str | None
    note: str | None
    uploaded_at: datetime
    uploaded_by: str | None
    expires_at: datetime


class DistributeRequest(BaseModel):
    """Payload for the 'distribute this file to these machines' endpoint."""
    machine_ids: list[int] = Field(..., min_length=1)


class FileDistributionRead(BaseModel):
    """Per-machine delivery status returned to the dashboard."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    file_id: int
    machine_id: int
    status: DistributionStatus
    bytes_received: int
    error_message: str | None
    created_at: datetime
    completed_at: datetime | None


class AgentPendingFile(BaseModel):
    """File entry returned to the agent when it asks 'what should I download?'"""
    distribution_id: int
    file_id: int
    storage_id: str
    filename: str
    sha256: str
    size_bytes: int


class AgentAckRequest(BaseModel):
    """Sent by the agent after a download attempt."""
    success: bool
    bytes_received: int = Field(..., ge=0)
    error_message: str | None = Field(default=None, max_length=2000)
