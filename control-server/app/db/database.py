"""
Database setup using SQLAlchemy.

Provides:
  - engine: the database connection
  - SessionLocal: factory for creating DB sessions
  - Base: parent class for all ORM models
  - get_db(): FastAPI dependency that yields a session
"""

from collections.abc import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from app.core.config import get_settings


_settings = get_settings()


# SQLite needs a special connect_arg to work with FastAPI's threading
# model. For other DBs (Postgres etc.) this is not needed.
_connect_args = (
    {"check_same_thread": False}
    if _settings.database_url.startswith("sqlite")
    else {}
)


engine = create_engine(
    _settings.database_url,
    connect_args=_connect_args,
    # Set echo=True only for debugging - very noisy
    echo=False,
)


SessionLocal = sessionmaker(
    bind=engine,
    autocommit=False,
    autoflush=False,
)


class Base(DeclarativeBase):
    """Base class for all ORM models."""
    pass


def get_db() -> Generator[Session, None, None]:
    """FastAPI dependency that yields a database session."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
