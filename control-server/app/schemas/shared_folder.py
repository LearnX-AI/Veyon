"""Request/response schemas for shared folder endpoints."""

from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field

from app.models.shared_folder import FolderStatus


# ---------------------------------------------------------------------------
# Admin schemas
# ---------------------------------------------------------------------------

class FolderCreate(BaseModel):
    """Body for POST /folders."""
    name: str = Field(..., min_length=1, max_length=255)
    description: str | None = Field(default=None, max_length=2000)
    deadline: datetime | None = None
    machine_ids: list[int] = Field(default_factory=list)


class FolderUpdate(BaseModel):
    """Body for PATCH /folders/{id}. All fields optional."""
    description: str | None = Field(default=None, max_length=2000)
    deadline: datetime | None = None
    status: FolderStatus | None = None


class FolderRead(BaseModel):
    """Returned by GET /folders. Includes summary counts."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    name: str
    description: str | None
    status: FolderStatus
    deadline: datetime | None
    created_at: datetime
    created_by: str | None

    # Summary counts (filled in by the endpoint, not from the model).
    machine_count: int = 0
    material_count: int = 0
    submission_count: int = 0


class FolderAssignRequest(BaseModel):
    """Body for POST /folders/{id}/assign."""
    machine_ids: list[int] = Field(..., min_length=1)


class SubmissionRead(BaseModel):
    """Returned by GET /folders/{id}/submissions."""
    model_config = ConfigDict(from_attributes=True)

    id: int
    folder_id: int
    machine_id: int
    filename: str
    sha256: str
    size_bytes: int
    content_type: str | None
    note: str | None
    submitted_at: datetime


# ---------------------------------------------------------------------------
# Agent schemas
# ---------------------------------------------------------------------------

class AgentFolderMaterial(BaseModel):
    """One material file the agent needs to keep in its materials/ dir."""
    distribution_id: int
    file_id: int
    filename: str
    sha256: str
    size_bytes: int


class AgentFolderState(BaseModel):
    """Per-folder state returned by GET /folders/sync."""
    folder_id: int
    name: str
    status: FolderStatus
    deadline: datetime | None
    materials: list[AgentFolderMaterial]
