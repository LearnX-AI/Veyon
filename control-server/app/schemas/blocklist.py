"""Request/response schemas for blocklist endpoints."""

import re
from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field, field_validator


# Conservative domain pattern: letters, digits, dots, hyphens.
# Rejects schemes (http://), paths (/foo), and whitespace.
_DOMAIN_PATTERN = re.compile(r"^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$")


class BlocklistEntryCreate(BaseModel):
    """Payload for adding a domain to the blocklist."""
    domain: str = Field(..., min_length=3, max_length=253)
    note: str | None = Field(default=None, max_length=255)

    @field_validator("domain")
    @classmethod
    def validate_domain(cls, v: str) -> str:
        v = v.strip().lower()
        if not _DOMAIN_PATTERN.match(v):
            raise ValueError(
                "Invalid domain. Use plain hostnames like 'facebook.com', "
                "no schemes (http://) or paths (/foo)."
            )
        return v


class BlocklistEntryRead(BaseModel):
    """A blocklist entry returned to the client."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    domain: str
    note: str | None
    created_at: datetime
    created_by: str | None
