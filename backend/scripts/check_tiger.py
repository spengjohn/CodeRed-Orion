"""Check the Tiger Data connection without switching the app over (dev tool).

Uses TIGER_DATABASE_URL from backend/.env. Run from backend/:
    python scripts/check_tiger.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # so "app" imports work

from app.data import tiger  # noqa: E402

latest = tiger.get_latest()
print("Latest reading:", latest if latest else "none yet (table is empty)")

history = tiger.get_history(range_s=600, bucket_s=60)
print(f"Last 10 minutes, 1-minute buckets: {len(history)} buckets")
for point in history:
    print(" ", point)
