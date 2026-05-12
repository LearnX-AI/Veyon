"""
Per-machine delivery records.

One row per (file, machine) target. Tracks whether the agent has picked it up,
finished downloading, or failed.
"""

from datetime import UTC, datetime
from enum import Enum as PyEnum

from sqlalchemy import DateTime, Enum, ForeignKey, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class DistributionStatus(str, PyEnum):
    """Lifecycle of a file delivery to one machine."""
    QUEUED      = "queued"
    DOWNLOADING = "downloading"
    DELIVERED   = "delivered"
    FAILED      = "failed"


class FileDistribution(Base):
    __tablename__ = "file_distributions"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    file_id:    Mapped[int] = mapped_column(
        Integer,
        ForeignKey("files.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )
    machine_id: Mapped[int] = mapped_column(
        Integer,
        ForeignKey("machines.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )

    status: Mapped[DistributionStatus] = mapped_column(
        Enum(DistributionStatus, native_enum=False, length=20),
        nullable=False,
        default=DistributionStatus.QUEUED,
        index=True,
    )

    # How many bytes the agent has successfully written. Bumped via ack
    # endpoint so the dashboard can show progress.
    bytes_received: Mapped[int] = mapped_column(Integer, nullable=False, default=0)

    error_message: Mapped[str | None] = mapped_column(Text, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
    completed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
