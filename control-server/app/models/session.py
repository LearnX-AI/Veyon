"""
Time-limited sessions for lab/exam.

A session is created by a teacher, assigned to a set of machines, then
started. Once running, the agent on each assigned PC counts down,
shows warnings at configured intervals, and executes the timeout
action when ends_at passes.
"""

from datetime import UTC, datetime
from enum import Enum as PyEnum

from sqlalchemy import (
    DateTime, Enum, Integer, JSON, String, Text,
)
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class SessionMode(str, PyEnum):
    LAB  = "lab"
    EXAM = "exam"


class SessionStatus(str, PyEnum):
    """Lifecycle of a session."""
    SCHEDULED = "scheduled"     # created, not started
    RUNNING   = "running"       # timer is ticking
    PAUSED    = "paused"        # frozen by teacher
    COMPLETED = "completed"     # timer expired, action fired
    CANCELLED = "cancelled"     # ended without action by teacher


class TimeoutAction(str, PyEnum):
    LOCK_SCREEN = "lock_screen"
    LOGOUT      = "logout"
    SHUTDOWN    = "shutdown"


class Session(Base):
    __tablename__ = "sessions"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    name: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)

    mode: Mapped[SessionMode] = mapped_column(
        Enum(SessionMode, native_enum=False, length=10),
        nullable=False,
        default=SessionMode.LAB,
    )

    duration_minutes: Mapped[int] = mapped_column(Integer, nullable=False)

    # JSON array of minutes-before-end at which to warn (e.g. [15, 5, 1]).
    warning_minutes: Mapped[list] = mapped_column(JSON, nullable=False, default=list)

    timeout_action: Mapped[TimeoutAction] = mapped_column(
        Enum(TimeoutAction, native_enum=False, length=20),
        nullable=False,
        default=TimeoutAction.LOCK_SCREEN,
    )

    status: Mapped[SessionStatus] = mapped_column(
        Enum(SessionStatus, native_enum=False, length=20),
        nullable=False,
        default=SessionStatus.SCHEDULED,
        index=True,
    )

    # Absolute wall-clock timestamps. Set on Start; updated on Pause/Resume/Extend.
    started_at:  Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    ends_at:     Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True, index=True)
    paused_at:   Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
    )
    created_by: Mapped[str | None] = mapped_column(String(100), nullable=True)
