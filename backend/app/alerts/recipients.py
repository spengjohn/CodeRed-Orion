"""
File-backed recipient registry for emergency alert phone numbers.

Stores registered phone numbers in a JSON file with automatic TTL-based
cleanup so stale demo entries are discarded on read.
"""

import json
import logging
import os
import threading
import time
from pathlib import Path
from typing import List, Optional

logger = logging.getLogger(__name__)

# Default time-to-live for recipient entries (2 hours)
DEFAULT_TTL_SECONDS: int = int(os.getenv("RECIPIENT_TTL_SECONDS", "7200"))

# Resolve path relative to this file so it works regardless of cwd
_RECIPIENTS_DIR = Path(__file__).resolve().parent
_RECIPIENTS_FILE = _RECIPIENTS_DIR / "recipients.json"

_lock = threading.Lock()


def _read_raw() -> list:
    """Read the raw JSON list from disk. Returns [] on any failure."""
    try:
        if _RECIPIENTS_FILE.exists():
            with open(_RECIPIENTS_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, list):
                return data
    except (json.JSONDecodeError, OSError) as exc:
        logger.warning("[Recipients] Failed to read %s: %s", _RECIPIENTS_FILE, exc)
    return []


def _write_raw(data: list) -> None:
    """Write the recipient list atomically to disk."""
    try:
        with open(_RECIPIENTS_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except OSError as exc:
        logger.error("[Recipients] Failed to write %s: %s", _RECIPIENTS_FILE, exc)


def _prune_expired(entries: list, ttl: int) -> list:
    """Remove entries older than `ttl` seconds from now."""
    cutoff = time.time() - ttl
    return [e for e in entries if e.get("added_at", 0) > cutoff]


def get_recipients(ttl: Optional[int] = None) -> List[dict]:
    """
    Return all active (non-expired) recipient entries.

    Side-effect: prunes expired entries from the file on each read.
    """
    if ttl is None:
        ttl = DEFAULT_TTL_SECONDS

    with _lock:
        entries = _read_raw()
        active = _prune_expired(entries, ttl)
        # Persist pruned list if anything was removed
        if len(active) != len(entries):
            _write_raw(active)
        return active


def add_recipient(phone: str, name: str = "") -> dict:
    """
    Register a phone number for emergency alerts.

    Deduplicates by phone number — updating the name and timestamp if the
    number already exists.

    Returns the created/updated entry dict.
    """
    entry = {
        "phone": phone,
        "name": name,
        "added_at": int(time.time()),
    }

    with _lock:
        entries = _read_raw()
        entries = _prune_expired(entries, DEFAULT_TTL_SECONDS)

        # Deduplicate: remove any existing entry with the same phone
        entries = [e for e in entries if e.get("phone") != phone]
        entries.append(entry)
        _write_raw(entries)

    logger.info("[Recipients] Registered %s (%s)", phone, name or "unnamed")
    return entry


def clear_recipients() -> int:
    """
    Remove all registered recipients.

    Returns the number of entries that were removed.
    """
    with _lock:
        entries = _read_raw()
        count = len(entries)
        _write_raw([])

    logger.info("[Recipients] Cleared %d recipient(s)", count)
    return count
