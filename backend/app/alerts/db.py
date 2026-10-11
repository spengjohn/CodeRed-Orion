"""
Decoupled Tiger Data (PostgreSQL) adapter for emergency alerts.

Uses the same database URL as tiger.py (TIGER_DATABASE_URL) while inserting
into a distinct alert table (defaults to 'emergency_events').

Does not auto-create the table; safely handles and logs any insertion or connection
errors so the emergency pipeline is never blocked or crashed.
"""

import json
import logging
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional
from dotenv import load_dotenv

# Explicitly load backend/.env
load_dotenv(Path(__file__).resolve().parent.parent.parent / ".env")

logger = logging.getLogger(__name__)

# Distinct table name from tiger.py's TIGER_TABLE ('infant_vitals')
DEFAULT_ALERT_TABLE: str = "emergency_events"


def _get_table_name() -> str:
    """Return the configured emergency alert table name."""
    return (
        os.getenv("TIGER_ALERT_TABLE")
        or os.getenv("TIGERDATA_ALERT_TABLE")
        or DEFAULT_ALERT_TABLE
    ).strip()


def _get_connection():
    """
    Create and return a PostgreSQL connection.

    Uses TIGER_DATABASE_URL (matching tiger.py) with fallback to TIGERDATA_URL.
    Supports either psycopg (v3) or psycopg2 depending on what is installed.
    """
    dsn = (os.getenv("TIGER_DATABASE_URL") or os.getenv("TIGERDATA_URL") or "").strip()
    if not dsn:
        return None

    # Try psycopg (v3, used in tiger.py) first, then fallback to psycopg2
    try:
        import psycopg
        return psycopg.connect(dsn)
    except ImportError:
        pass
    except Exception as exc:
        logger.warning("[DB] Failed to connect via psycopg: %s", exc)
        return None

    try:
        import psycopg2
        return psycopg2.connect(dsn)
    except ImportError:
        logger.warning("[DB] Neither psycopg nor psycopg2 is installed.")
        return None
    except Exception as exc:
        logger.warning("[DB] Failed to connect via psycopg2: %s", exc)
        return None


def save_emergency_alert(
    emergency_type: str,
    details: dict,
    snapshot_bytes: bytes,
    timestamp: int,
) -> bool:
    """
    Persist an emergency alert record to the database table.

    Does not auto-create the table. If TIGER_DATABASE_URL is not set or
    the table does not exist, logs the issue and returns False without crashing.
    """
    record = {
        "emergency_type": emergency_type,
        "details": details if isinstance(details, dict) else {},
        "snapshot": snapshot_bytes,
        "timestamp": timestamp,
        "received_at": datetime.now(timezone.utc),
    }

    conn = _get_connection()
    table = _get_table_name()

    if conn is None:
        logger.info(
            "[DB] TIGER_DATABASE_URL not set or unreachable — alert record logged locally only. "
            "table=%s type=%s ts=%d",
            table,
            emergency_type,
            timestamp,
        )
        return False

    try:
        # Wrap raw bytes in psycopg2.Binary if psycopg2 connection is in use
        snapshot_param = record["snapshot"]
        if "psycopg2" in conn.__class__.__module__:
            try:
                import psycopg2
                snapshot_param = psycopg2.Binary(record["snapshot"])
            except Exception:
                pass

        with conn:
            with conn.cursor() as cur:
                cur.execute(
                    f"""
                    INSERT INTO {table}
                        (emergency_type, details, snapshot, edge_timestamp, received_at)
                    VALUES (%s, %s, %s, to_timestamp(%s), %s)
                    """,
                    (
                        record["emergency_type"],
                        json.dumps(record["details"]),
                        snapshot_param,
                        record["timestamp"],
                        record["received_at"],
                    ),
                )
        logger.info("[DB] Saved emergency alert to '%s': type=%s ts=%d", table, emergency_type, timestamp)
        return True

    except Exception as exc:
        logger.error("[DB] Failed to save alert to '%s': %s", table, exc)
        return False

    finally:
        try:
            conn.close()
        except Exception:
            pass

