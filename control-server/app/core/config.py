"""
Application configuration.

Loads settings from environment variables (and .env file in development).
Never commit the .env file - it contains secrets.
"""

from functools import lru_cache
from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """Typed application settings."""

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    # ---- Server ----
    app_name: str = "Veyon Control Server"
    app_version: str = "0.1.0"
    host: str = "0.0.0.0"
    port: int = Field(default=8000, ge=1, le=65535)

    # ---- Database ----
    database_url: str = "sqlite:///./veyon_control.db"

    # ---- Security ----
    # Token used by the dashboard / clients to authenticate.
    # MUST be set in production via .env. This default is only
    # for development convenience and will trigger a warning.
    admin_token: str = "CHANGE_ME_IN_PRODUCTION"

    # Rate limit defaults (per IP)
    rate_limit_default: str = "60/minute"
    rate_limit_strict: str = "10/minute"

    # ---- CORS ----
    # Origins allowed to call the API. In production set this to
    # the dashboard's URL only.
    cors_origins: list[str] = Field(default_factory=lambda: ["*"])

    @property
    def is_default_token(self) -> bool:
        return self.admin_token == "CHANGE_ME_IN_PRODUCTION"


@lru_cache
def get_settings() -> Settings:
    """Cached singleton settings accessor."""
    return Settings()
