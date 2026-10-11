"""Seed Tiger Data with 30 days of realistic history (dev/demo tool).

Writes one row per minute, ending one minute before the earliest real row, so
seeded data never overlaps or mixes with what the board wrote.

Uses TIGER_DATABASE_URL and TIGER_TABLE from backend/.env. Run from backend/:
    python scripts/seed_tiger.py            # dry run: shows what would be inserted
    python scripts/seed_tiger.py --apply    # actually insert

Undo: every seeded row is older than the first real row, so
    DELETE FROM <table> WHERE unix_time < <earliest real unix_time>
removes exactly the seed (the script prints the exact statement).
"""

import math
import os
import random
import sys
from datetime import datetime, timezone

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # so "app" imports work

from app.data import tiger  # noqa: E402

DAYS = 30
STEP_S = 60  # one row per minute, like the board
COLUMNS = "time, unix_time, weight, ambient_temp, baby_temp, breaths, moved"


def make_row(t, start, end, rng):
    """One realistic reading for unix second t (all values rounded to 2 decimals like the board)."""
    day_phase = 2 * math.pi * ((t % 86400) / 86400)  # 0..2pi over a UTC day
    progress = (t - start) / (end - start)            # 0 at the start of the seed, 1 at the end

    weight = 6.1 + 0.9 * progress + rng.uniform(-0.02, 0.02)                                   # ~30 g/day growth
    ambient = 22.5 + 1.2 * math.sin(day_phase) + 0.3 * math.sin(2 * math.pi * t / 7200) + rng.uniform(-0.1, 0.1)
    baby_temp = 36.8 + 0.2 * math.sin(day_phase) + rng.uniform(-0.05, 0.05)
    breaths = round(40 + 4 * math.sin(2 * math.pi * t / 600) + rng.uniform(-2, 2))             # integer column
    moved = rng.random() < 0.08                                                                  # wriggles ~8% of minutes

    return (
        datetime.fromtimestamp(t, tz=timezone.utc), t,
        round(weight, 2), round(ambient, 2), round(baby_temp, 2), breaths, moved,
    )


def main():
    apply = "--apply" in sys.argv
    table = tiger._query("{table}")  # validates TIGER_TABLE and quotes it safely

    with tiger._get_pool().connection() as conn:
        earliest = conn.execute(tiger._query("SELECT min(unix_time) FROM {table}")).fetchone()[0]
        if earliest is None:
            sys.exit("The table is empty; there is no earliest row to seed before.")

        end = earliest - STEP_S                        # one minute before the earliest real row
        start = end - (DAYS * 1440 - 1) * STEP_S       # 30 days of minutes, inclusive of end
        existing = conn.execute(
            tiger._query("SELECT count(*) FROM {table} WHERE unix_time BETWEEN %s AND %s"), (start, end)
        ).fetchone()[0]

        rng = random.Random(42)  # fixed seed: the same run always produces the same data
        rows = [make_row(t, start, end, rng) for t in range(start, end + 1, STEP_S)]

        print(f"Table: {table.as_string(conn)}")
        print(f"Earliest real row: {earliest} ({datetime.fromtimestamp(earliest, tz=timezone.utc).isoformat()})")
        print(f"Seed range: {rows[0][0].isoformat()} -> {rows[-1][0].isoformat()}  ({len(rows)} rows)")
        print("First row:", rows[0])
        print("Last row: ", rows[-1])
        print(f"Rows already in that range: {existing}")

        if existing:
            sys.exit("Refusing to seed: the range already has rows (was the seed already run?).")
        if not apply:
            print("\nDry run only, nothing written. Re-run with --apply to insert.")
            return

        copy_sql = tiger._query(f"COPY {{table}} ({COLUMNS}) FROM STDIN")
        with conn.cursor() as cur, cur.copy(copy_sql) as copy:
            for row in rows:
                copy.write_row(row)
        # the pool commits the transaction when the "with" block exits without an error

    with tiger._get_pool().connection() as conn:
        inserted = conn.execute(
            tiger._query("SELECT count(*) FROM {table} WHERE unix_time BETWEEN %s AND %s"), (start, end)
        ).fetchone()[0]
    print(f"\nInserted {inserted} rows.")
    print(f"To undo: DELETE FROM {table.as_string(None)} WHERE unix_time < {earliest};")


if __name__ == "__main__":
    main()
