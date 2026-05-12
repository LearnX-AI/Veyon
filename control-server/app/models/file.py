"""
Stored file metadata.

The actual file bytes live on disk at /var/lib/veyon-server/uploads/<uuid>/<filename>.
This table just tracks what's there, who uploaded it, and when it expires.
"""

from datetime import UTC, datetime

from sqlalchemy import BigInteger, DateTime, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class FileRecord(Base):
    __tablename__ = "files"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    # Server-generated UUID used as the directory name on disk.
    storage_id: Mapped[str] = mapped_column(String(36), nullable=False, unique=True, index=True)

    # Original filename from the uploader (sanitized for safe filesystem use).
    filename: Mapped[str] = mapped_column(String(255), nullable=False)

    # SHA-256 of the uploaded content, used to detect duplicate uploads
    # and verify integrity on the agent side.
    sha256: Mapped[str] = mapped_column(String(64), nullable=False, index=True)

    size_bytes: Mapped[int] = mapped_column(BigInteger, nullable=False)
    content_type: Mapped[str | None] = mapped_column(String(127), nullable=True)
    note: Mapped[str | None] = mapped_column(Text, nullable=True)

    uploaded_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
        index=True,
    )
    uploaded_by: Mapped[str | None] = mapped_column(String(100), nullable=True)

    # When the file becomes eligible for auto-deletion (hybrid storage policy).
    expires_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        index=True,
    )
