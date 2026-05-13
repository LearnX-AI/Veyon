"""
Folder ↔ Machine assignments.

A folder is "assigned" to a set of student machines. Only those machines
see the folder, can download its materials, and can upload submissions
into it.
"""

from datetime import UTC, datetime

from sqlalchemy import DateTime, ForeignKey, Integer, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class FolderAssignment(Base):
    __tablename__ = "folder_assignments"
    __table_args__ = (
        UniqueConstraint("folder_id", "machine_id", name="uq_folder_machine"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    folder_id: Mapped[int] = mapped_column(
        Integer,
        ForeignKey("shared_folders.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )
    machine_id: Mapped[int] = mapped_column(
        Integer,
        ForeignKey("machines.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )

    assigned_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
