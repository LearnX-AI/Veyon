"""
Shared folders - logical assignment containers.

A folder holds:
  - Materials: files distributed FROM teacher TO students (uses existing
    file_distributions, tagged with folder_id)
  - Submissions: files coming FROM students INTO the folder

Lifecycle: OPEN (accepting submissions) -> CLOSED (deadline passed,
no more submissions accepted).
"""

from datetime import UTC, datetime
from enum import Enum as PyEnum

from sqlalchemy import DateTime, Enum, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class FolderStatus(str, PyEnum):
    OPEN   = "open"
    CLOSED = "closed"


class SharedFolder(Base):
    __tablename__ = "shared_folders"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    # User-facing name, e.g. "Grade 10 Math - Assignment 5".
    # Also used (sanitized) as the on-disk folder name on student PCs.
    name: Mapped[str] = mapped_column(String(255), nullable=False, unique=True, index=True)

    description: Mapped[str | None] = mapped_column(Text, nullable=True)

    status: Mapped[FolderStatus] = mapped_column(
        Enum(FolderStatus, native_enum=False, length=20),
        nullable=False,
        default=FolderStatus.OPEN,
        index=True,
    )

    # Optional deadline. When reached, status flips to CLOSED automatically
    # via a periodic check, and student agents stop accepting submissions.
    deadline: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
    created_by: Mapped[str | None] = mapped_column(String(100), nullable=True)
