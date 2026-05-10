"""
Blocklist entry model.

Each row represents one blocked domain.
"""

from datetime import UTC, datetime

from sqlalchemy import DateTime, Integer, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class BlocklistEntry(Base):
    __tablename__ = "blocklist_entries"
    __table_args__ = (UniqueConstraint("domain", name="uq_blocklist_domain"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    domain: Mapped[str] = mapped_column(String(253), nullable=False, index=True)
    note: Mapped[str | None] = mapped_column(String(255), nullable=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
    created_by: Mapped[str | None] = mapped_column(String(100), nullable=True)
