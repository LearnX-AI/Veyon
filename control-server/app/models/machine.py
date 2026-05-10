"""
Machine registry.

Each row represents a known student PC. The sync agent on each PC
periodically calls /machines/heartbeat to update last_seen.
"""

from datetime import UTC, datetime

from sqlalchemy import Boolean, DateTime, Integer, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class Machine(Base):
    __tablename__ = "machines"
    __table_args__ = (UniqueConstraint("hostname", name="uq_machine_hostname"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    hostname: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    ip_address: Mapped[str | None] = mapped_column(String(45), nullable=True)
    label: Mapped[str | None] = mapped_column(String(100), nullable=True)
    focus_mode_active: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    blocklist_version: Mapped[int] = mapped_column(Integer, nullable=False, default=0)
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    registered_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
