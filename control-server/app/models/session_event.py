"""
Session lifecycle events - feeds the 'usage history tracking' UI.

Every meaningful thing that happens to a session is logged here:
who/what triggered it, on which machine (if applicable), with details.
"""

from datetime import UTC, datetime
from enum import Enum as PyEnum

from sqlalchemy import DateTime, Enum, ForeignKey, Integer, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.db.database import Base


class SessionEventType(str, PyEnum):
    STARTED      = "started"        # session began
    WARNED       = "warned"         # agent showed a warning to a user
    PAUSED       = "paused"
    RESUMED      = "resumed"
    EXTENDED     = "extended"
    ACTION_FIRED = "action_fired"   # agent executed the timeout action
    COMPLETED    = "completed"      # session finished naturally
    CANCELLED    = "cancelled"      # teacher ended early without action


class SessionEvent(Base):
    __tablename__ = "session_events"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)

    session_id: Mapped[int] = mapped_column(
        Integer,
        ForeignKey("sessions.id", ondelete="CASCADE"),
        nullable=False,
        index=True,
    )

    # NULL when the event applies to the whole session (e.g. Started).
    # Non-NULL when machine-specific (e.g. Warned-on-machine-3).
    machine_id: Mapped[int | None] = mapped_column(
        Integer,
        ForeignKey("machines.id", ondelete="SET NULL"),
        nullable=True,
        index=True,
    )

    event_type: Mapped[SessionEventType] = mapped_column(
        Enum(SessionEventType, native_enum=False, length=20),
        nullable=False,
        index=True,
    )

    details: Mapped[str | None] = mapped_column(Text, nullable=True)

    occurred_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        nullable=False,
        default=lambda: datetime.now(UTC),
        index=True,
    )
