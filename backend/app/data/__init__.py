"""Data source switch.

The rest of the backend only imports get_latest / get_history from here.
When Tiger Data is ready, comment out the dummy line and uncomment the tiger line.
"""

from .dummy import get_latest, get_history     # dummy data (current)
# from .tiger import get_latest, get_history   # live Tiger Data (set TIGER_DATABASE_URL in backend/.env first)
