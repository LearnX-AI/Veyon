"""Schema for the audit log."""

from datetime import datetime

from pydantic import BaseModel, ConfigDict


class AdminActionRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    action: str
    target: str | None
    details: str | None
    actor_ip: str | None
    occurred_at: datetime
