"""
On-disk file storage helpers.

Files live at <storage_root>/<storage_id>/<original_filename>.
The storage_id is a UUID4 directory generated per upload, so:
  - filename collisions are impossible
  - we can wipe a single file by removing one directory
  - the filename on disk matches the original (easier debugging)

All I/O is chunked to handle 1GB uploads without exhausting memory.
"""

from __future__ import annotations

import asyncio
import hashlib
import logging
import os
import re
import shutil
import uuid
from datetime import UTC, datetime
from pathlib import Path
from typing import AsyncIterator, BinaryIO

from app.core.config import get_settings


_log = logging.getLogger("uvicorn")
CHUNK_SIZE = 1024 * 1024   # 1 MB read/write chunks


def _settings():
    return get_settings()


def _safe_filename(raw: str) -> str:
    """
    Strip directory parts, control chars, and dangerous characters.
    We never trust filenames from the client.
    """
    # Take only the basename (drops any path traversal)
    name = os.path.basename(raw or "").strip()
    # Replace anything not in a conservative allow-list
    name = re.sub(r"[^A-Za-z0-9._\-+ ]+", "_", name)
    # Limit length to 200 chars to leave room for path components
    name = name[:200]
    if not name or name in (".", ".."):
        name = "unnamed.bin"
    return name


def make_storage_path(storage_id: str, filename: str) -> Path:
    """Compute the full disk path for a stored file."""
    root = Path(_settings().file_storage_root)
    return root / storage_id / filename


def ensure_storage_root() -> None:
    """Create the storage root if missing. Idempotent."""
    root = Path(_settings().file_storage_root)
    root.mkdir(parents=True, exist_ok=True)


async def save_upload_stream(
    upload_stream: AsyncIterator[bytes],
    declared_filename: str,
    max_bytes: int | None = None,
) -> tuple[str, str, str, int]:
    """
    Stream an upload to disk, computing SHA-256 and enforcing size limits.

    Returns: (storage_id, safe_filename, sha256_hex, total_bytes)

    Raises:
        ValueError if max_bytes is exceeded
    """
    settings = _settings()
    limit = max_bytes if max_bytes is not None else settings.file_max_size_bytes

    storage_id = str(uuid.uuid4())
    safe_name = _safe_filename(declared_filename)
    target = make_storage_path(storage_id, safe_name)
    target.parent.mkdir(parents=True, exist_ok=True)

    hasher = hashlib.sha256()
    total = 0

    try:
        # aiofiles would be cleaner but adds a dep; sync I/O in a thread is fine
        # for our size range (1GB max, single upload at a time per machine).
        def _write_sync(chunk: bytes) -> None:
            nonlocal total
            total += len(chunk)
            if total > limit:
                raise ValueError(f"Upload exceeded size limit of {limit} bytes")
            hasher.update(chunk)
            _f.write(chunk)

        with open(target, "wb") as _f:
            async for chunk in upload_stream:
                if not chunk:
                    continue
                # Run the sync write off the event loop to keep things responsive
                await asyncio.to_thread(_write_sync, chunk)

    except Exception:
        # Clean up partial file on any failure
        shutil.rmtree(target.parent, ignore_errors=True)
        raise

    return storage_id, safe_name, hasher.hexdigest(), total


async def iter_file_chunks(storage_id: str, filename: str) -> AsyncIterator[bytes]:
    """Stream a stored file back, one chunk at a time."""
    path = make_storage_path(storage_id, filename)
    if not path.is_file():
        raise FileNotFoundError(str(path))

    def _read_next(f: BinaryIO) -> bytes:
        return f.read(CHUNK_SIZE)

    with open(path, "rb") as f:
        while True:
            chunk = await asyncio.to_thread(_read_next, f)
            if not chunk:
                break
            yield chunk


def delete_file(storage_id: str) -> bool:
    """Remove a stored file's directory. Returns True if anything was deleted."""
    root = Path(_settings().file_storage_root)
    target = root / storage_id
    if not target.exists():
        return False
    shutil.rmtree(target, ignore_errors=True)
    return True


def cleanup_expired_files(now: datetime | None = None) -> dict[str, int]:
    """
    Delete files past their expires_at: removes disk content AND DB rows
    (which cascades to file_distributions).

    Runs in a fresh DB session so it's safe to call from a background task.
    Returns: {"removed": N, "skipped": M}
      removed = successfully cleaned
      skipped = errors (logged), not deleted
    """
    from app.db.database import SessionLocal
    from app.models import FileRecord

    when = now or datetime.now(UTC)
    removed = 0
    skipped = 0

    db = SessionLocal()
    try:
        # Find all expired records
        from sqlalchemy import select
        stmt = select(FileRecord).where(FileRecord.expires_at < when)
        expired = list(db.scalars(stmt))

        for rec in expired:
            try:
                delete_file(rec.storage_id)   # remove bytes from disk
                db.delete(rec)                # remove DB row (cascades)
                removed += 1
            except Exception as exc:
                _log.warning(
                    "Cleanup failed for file %s (storage_id=%s): %s",
                    rec.id, rec.storage_id, exc,
                )
                skipped += 1

        if removed > 0 or skipped > 0:
            db.commit()
    finally:
        db.close()

    return {"removed": removed, "skipped": skipped}


# ============================================================
# Submissions storage (folder feature)
# ============================================================
#
# Submissions live under a separate root from regular files so the
# auto-cleanup janitor never touches them (submissions don't expire).
# Layout: <submission_root>/<storage_id>/<safe-filename>

def submission_storage_path(storage_id: str, filename: str) -> Path:
    """Disk path for a submission file."""
    root = Path(_settings().submission_storage_root)
    return root / storage_id / filename


def ensure_submission_root() -> None:
    """Create the submission storage root if missing. Idempotent."""
    Path(_settings().submission_storage_root).mkdir(parents=True, exist_ok=True)


async def save_submission_stream(
    upload_stream: AsyncIterator[bytes],
    declared_filename: str,
    max_bytes: int | None = None,
) -> tuple[str, str, str, int]:
    """
    Same shape as save_upload_stream() but writes under the submission root.
    Returns: (storage_id, safe_filename, sha256_hex, total_bytes)
    """
    settings = _settings()
    limit = max_bytes if max_bytes is not None else settings.file_max_size_bytes

    storage_id = str(uuid.uuid4())
    safe_name = _safe_filename(declared_filename)
    target = submission_storage_path(storage_id, safe_name)
    target.parent.mkdir(parents=True, exist_ok=True)

    hasher = hashlib.sha256()
    total = 0

    try:
        def _write_sync(chunk: bytes) -> None:
            nonlocal total
            total += len(chunk)
            if total > limit:
                raise ValueError(f"Submission exceeded size limit of {limit} bytes")
            hasher.update(chunk)
            _f.write(chunk)

        with open(target, "wb") as _f:
            async for chunk in upload_stream:
                if not chunk:
                    continue
                await asyncio.to_thread(_write_sync, chunk)

    except Exception:
        shutil.rmtree(target.parent, ignore_errors=True)
        raise

    return storage_id, safe_name, hasher.hexdigest(), total


async def iter_submission_chunks(storage_id: str, filename: str) -> AsyncIterator[bytes]:
    """Stream a submission file back. Used when teachers download submissions."""
    path = submission_storage_path(storage_id, filename)
    if not path.is_file():
        raise FileNotFoundError(str(path))

    def _read_next(f: BinaryIO) -> bytes:
        return f.read(CHUNK_SIZE)

    with open(path, "rb") as f:
        while True:
            chunk = await asyncio.to_thread(_read_next, f)
            if not chunk:
                break
            yield chunk


def delete_submission(storage_id: str) -> bool:
    """Remove a submission's disk directory. Returns True if anything was deleted."""
    root = Path(_settings().submission_storage_root)
    target = root / storage_id
    if not target.exists():
        return False
    shutil.rmtree(target, ignore_errors=True)
    return True
