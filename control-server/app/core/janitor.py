"""
Background janitor: periodically removes expired files.

Runs as an asyncio task inside the FastAPI process - no separate
scheduler or cron needed. Started on app startup, cancelled on shutdown.
"""

from __future__ import annotations

import asyncio
import logging

from app.core.config import get_settings
from app.core.file_storage import cleanup_expired_files


_log = logging.getLogger("uvicorn")


async def run_cleanup_loop() -> None:
    """Forever: sleep for the configured interval, then run one cleanup pass."""
    interval = get_settings().file_cleanup_interval_seconds
    _log.info("File cleanup janitor started (interval=%ds)", interval)

    while True:
        try:
            await asyncio.sleep(interval)
            # Run the (synchronous) cleanup in a thread so we don't block
            # the event loop while DB / disk I/O happens.
            result = await asyncio.to_thread(cleanup_expired_files)
            if result["removed"] or result["skipped"]:
                _log.info(
                    "File cleanup: removed=%d skipped=%d",
                    result["removed"], result["skipped"],
                )
        except asyncio.CancelledError:
            _log.info("File cleanup janitor stopped")
            raise
        except Exception as exc:
            # Never let the janitor die. Log and keep going on next tick.
            _log.exception("File cleanup tick failed: %s", exc)
