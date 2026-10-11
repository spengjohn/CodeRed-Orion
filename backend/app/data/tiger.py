"""Tiger Data source: reads the readings the crib board writes to Tiger Data.

Same two functions as dummy.py, returning the same JSON shape via to_reading(),
so switching in app/data/__init__.py needs no other changes.

Expected table (one crib, written by the board about once a minute; columns
match the board's INSERT in edge/src/net/TimeseriesClient.cpp):
    time                   -- timestamp (not used here; unix_time is)
    unix_time              -- unix seconds
    weight                 -- kg
    ambient_temp           -- C
    baby_temp              -- C
    breaths                -- breaths per minute (integer)
    moved                  -- boolean, set by the board

Config (backend/.env, see .env.example):
    TIGER_DATABASE_URL     -- Postgres connection string from the Tiger Data console
    TIGER_TABLE            -- table name
"""

import os
import threading
import time
from pathlib import Path

from dotenv import load_dotenv
from psycopg import sql
from psycopg_pool import ConnectionPool

from .format import to_reading

load_dotenv(Path(__file__).resolve().parent.parent.parent / ".env")  # backend/.env

DB_TIMEOUT_S = 5  # give up after this long instead of hanging if Tiger Data is unreachable

# Columns in to_reading() order. Casts keep the JSON types stable:
#   ::float8           -> plain floats (numeric columns would otherwise come back as
#                         Decimal, which Flask's jsonify turns into a string)
#   moved::int <> 0    -> true/false whether the column is boolean, integer or float
LATEST_SQL = """
    SELECT unix_time::bigint,
           weight::float8,
           ambient_temp::float8,
           baby_temp::float8,
           breaths::float8,
           moved::int <> 0
    FROM {table}
    ORDER BY unix_time DESC
    LIMIT 1
"""

# time_bucket() is TimescaleDB's GROUP BY-friendly rounding: time_bucket(60, t)
# puts every row into the 60-second bucket that starts at or before t.
# Vitals are averaged per bucket; moved is true if the baby moved at all in it.
# NULLIF(x, 'NaN') makes avg() skip a NaN reading instead of turning the whole
# bucket's average into NaN.
HISTORY_SQL = """
    SELECT time_bucket(%(bucket)s::bigint, unix_time::bigint) AS bucket_start,
           avg(NULLIF(weight, 'NaN'))::float8,
           avg(NULLIF(ambient_temp, 'NaN'))::float8,
           avg(NULLIF(baby_temp, 'NaN'))::float8,
           avg(breaths)::float8,
           bool_or(moved::int <> 0)
    FROM {table}
    WHERE unix_time >= %(since)s
    GROUP BY bucket_start
    ORDER BY bucket_start
"""

_pool = None
_pool_lock = threading.Lock()


def _query(template):
    """Fill the table name into a query (safely quoted, never pasted in as raw text)."""
    table = os.getenv("TIGER_TABLE", "").strip()
    if not table:
        raise RuntimeError("TIGER_TABLE is not set; add the table name to backend/.env (see .env.example)")
    return sql.SQL(template).format(table=sql.Identifier(table))


def _get_pool():
    """Create the connection pool on first use, so importing this file never connects."""
    global _pool
    with _pool_lock:
        if _pool is None:
            url = os.getenv("TIGER_DATABASE_URL", "").strip()
            if not url:
                raise RuntimeError("TIGER_DATABASE_URL is not set; add it to backend/.env (see .env.example)")
            _pool = ConnectionPool(
                url,
                min_size=1,
                max_size=4,
                timeout=DB_TIMEOUT_S,                   # wait at most this long for a connection
                kwargs={"connect_timeout": DB_TIMEOUT_S},
                check=ConnectionPool.check_connection,  # replace connections the server dropped
                open=True,
            )
        return _pool


def get_latest():
    """Most recent reading, or None if the table is still empty."""
    query = _query(LATEST_SQL)
    with _get_pool().connection() as conn:
        row = conn.execute(query).fetchone()
    return to_reading(*row) if row else None


def get_history(range_s=3600, bucket_s=60):
    """Readings for the last range_s seconds, averaged into bucket_s-second buckets.

    Same output as dummy.get_history(), except buckets with no rows are skipped
    (gaps) instead of being filled in. The board writes about once a minute, so
    use buckets of 60 s or more.
    """
    query = _query(HISTORY_SQL)
    now = int(time.time())
    start = now - range_s
    since = start - start % bucket_s  # align to bucket boundaries, like dummy.py
    with _get_pool().connection() as conn:
        rows = conn.execute(query, {"bucket": bucket_s, "since": since}).fetchall()
    return [to_reading(*row) for row in rows]
