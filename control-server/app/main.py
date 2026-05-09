"""
Veyon Control Server - FastAPI entry point.

Run locally with:
    uvicorn app.main:app --reload
"""

import logging

from pathlib import Path

from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address

from app.api import api_router
from app.core.config import get_settings
from app.db.database import Base, engine


_log = logging.getLogger("uvicorn")
_settings = get_settings()


# Rate limiter: limits per-IP by default
limiter = Limiter(key_func=get_remote_address, default_limits=[_settings.rate_limit_default])


def create_app() -> FastAPI:
    """Application factory."""

    # Create database tables on startup if missing.
    # In production we'd use Alembic migrations - this is fine for v1.
    Base.metadata.create_all(bind=engine)

    if _settings.is_default_token:
        _log.warning(
            "ADMIN_TOKEN is set to the default placeholder. "
            "Set a real value in .env before running in production."
        )

    app = FastAPI(
        title=_settings.app_name,
        version=_settings.app_version,
        docs_url="/docs",
        redoc_url="/redoc",
    )

    # Rate limiting
    app.state.limiter = limiter
    app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

    # CORS
    app.add_middleware(
        CORSMiddleware,
        allow_origins=_settings.cors_origins,
        allow_credentials=True,
        allow_methods=["GET", "POST", "PUT", "DELETE", "OPTIONS"],
        allow_headers=["*"],
    )

    # Health check (public, no auth)
    @app.get("/health", tags=["health"])
    def health() -> dict[str, str]:
        return {"status": "ok", "version": _settings.app_version}

    # All real routes live under /api/v1/...
    app.include_router(api_router)

    # ---- Static dashboard ----
    static_dir = Path(__file__).parent.parent / "static"
    if static_dir.is_dir():
        app.mount("/static", StaticFiles(directory=static_dir), name="static")

        @app.get("/", include_in_schema=False)
        def dashboard() -> FileResponse:
            """Serve the admin dashboard."""
            return FileResponse(static_dir / "index.html")

    return app


app = create_app()
