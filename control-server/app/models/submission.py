"""
Submissions - files uploaded BY student machines INTO a shared folder.

Each submission is a file with provenance: which folder it was put in,
which machine uploaded it, when. Stored on the server alongside regular
files but in a separate disk subtree to keep things organized.
"""

from datetime import UTC, datetime

from sqlalchemy import BigInteger, DateTime, ForeignKey, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class Submission(Base):
    __tablename__ = "submissions"

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

    # Storage info - mirrors FileRecord layout but under a separate root.
    storage_id: Mapped[str] = mapped_column(String(36), nullable=False, unique=True)
    filename:   Mapped[str] = mapped_column(String(255), nullable=False)
    sha256:     Mapped[str] = mapped_column(String(64), nullable=False)
    size_bytes: Mapped[int] = mapped_column(BigInteger, nullable=False)
    content_type: Mapped[str | None] = mapped_column(String(127), nullable=True)
    note:       Mapped[str | None] = mapped_column(Text, nullable=True)

    submitted_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
        index=True,
    )
