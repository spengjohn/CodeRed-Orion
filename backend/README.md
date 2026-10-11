# Backend (Flask)

Serves crib readings to the frontend. Uses **dummy data** for now; Tiger Data comes later.

## Run
From `backend/`:
```
python -m venv venv
venv\Scripts\activate          # macOS/Linux: source venv/bin/activate
pip install -r requirements.txt
copy .env.example .env         # macOS/Linux: cp .env.example .env  (then fill in values)
flask --app app run --debug    # http://localhost:5000  (or: python run.py)
```

### `.env` (create it for development)
Each developer creates their own `backend/.env` by copying `.env.example` and filling in the values (ask the team for them). The EC2 server needs its own `backend/.env` too. It holds secrets, so it is in `.gitignore` and must never be committed.
- Auth0: `AUTH0_DOMAIN`, `AUTH0_AUDIENCE`
- Tiger Data: `TIGER_DATABASE_URL` (connection string from the Tiger Data console), `TIGER_TABLE=infant_vitals`

Without `.env` the app still runs on dummy data; protected routes and the live push reject everyone (no Auth0), and the Tiger Data source can't connect.

## Files
- `app/__init__.py`: creates the Flask app, turns on CORS, registers routes and Socket.IO.
- `app/api/auth_routes.py`: `/api/public` (no login) and `/api/private-crib-data` (needs an Auth0 Bearer token).
- `app/auth/auth.py`: Auth0 JWT validation; `@require_auth` protects a route.
- `app/data/__init__.py`: data source switch. Comment out the dummy import and uncomment the Tiger Data import to go live.
- `app/data/dummy.py`: generates realistic fake readings from the timestamp (same second gives the same values).
- `app/data/tiger.py`: reads the same readings from Tiger Data (latest row, and `time_bucket()` averages for history). Expects the board's columns `unix_time`, `weight`, `ambient_temp`, `baby_temp`, `breaths`, `moved`.
- `app/data/format.py`: `to_reading()`, the one place that defines the JSON shape below.
- `app/api/readings.py`: REST endpoints `/api/readings/latest` and `/api/readings/history`.
- `app/realtime/__init__.py`: checks for a new reading every 30 s and pushes it to all connected clients only when it is new.
- `scripts/check_tiger.py`: tests the Tiger Data connection and prints the latest reading and last 10 minutes.
- `scripts/watch_vitals.py`: terminal client that prints pushed readings (needs `pip install "python-socketio[client]"`). Sends `AUTH0_TEST_TOKEN` from `.env` to pass the live push's login check; without a token, use `curl` on `/latest` instead.

## JSON for the frontend
One reading (`ts` = unix seconds, weight in kg, temperatures in °C; all values are numbers with up to 2 decimals):
```json
{ "ts": 1791663137, "babyWeightKg": 4.21, "ambientTempC": 23.04, "babyTempC": 36.87, "breathsPerMin": 38.47, "isMoving": false }
```

`GET /api/readings/latest` returns one reading (`404 {"error": "no readings yet"}` if the table is empty).

`GET /api/readings/history?range=3600&bucket=60` (seconds; max range 30 days, max 2000 points):
```json
{ "range": 3600, "bucket": 60, "series": [ { "ts": 1791659340, "babyWeightKg": 4.2, "ambientTempC": 22.13, "babyTempC": 36.95, "breathsPerMin": 36.21, "isMoving": false } ] }
```
Each series point is the average over its bucket (rounded to 2 decimals); `ts` is the start of the bucket. `isMoving` is true if the baby moved at any point in the bucket. With Tiger Data, buckets with no readings are skipped, so the chart may have gaps; the board writes about once a minute, so use buckets of 60 s or more.

## Live push (Socket.IO)
How it works (`app/realtime/__init__.py`):
```
board ──writes a row every ~60 s──▶ Tiger Data
                                        ▲
          every 30 s: read newest row ──┘
          new ts?  yes → emit "vitals" to all clients, remember ts
                   no  → do nothing
```
- The backend stores no readings; it only remembers the `ts` of the last row it pushed.
- Reading every 30 s while the board writes every 60 s means each new row is pushed exactly once, at most 30 s after it was written (with dummy data: one push every 30 s).
- A client that connects gets the newest row immediately, then only new ones. If the board stops writing, no more events arrive, so show the reading's age (from `ts`) in the UI.
- The loop starts on the first connected client and runs from then on. Database errors are logged and retried on the next check.
- Connecting requires an Auth0 access token; connections without a valid one are rejected.
```ts
import { io } from "socket.io-client";
const socket = io("http://localhost:5000", { auth: { token } });  // token from Auth0 login
socket.on("vitals", (reading) => { /* same shape as /latest */ });
```
Load charts once from `/history`, then append each pushed reading. Ignore a reading if its `ts` isn't newer than the last one.

## Alerts (for the alerts page)
When the board sends an alert, the backend saves it to the `alerts` table, calls every registered number (Twilio), and pushes it live. All alert routes need the Auth0 token.

| Code | Meaning |
|---|---|
| 1 | baby's temperature is below room temperature |
| 2 | baby's temperature is at room temperature |
| 3 | baby's temperature is high |
| 4 | baby's breathing rate is very low |
| 5 | baby's breathing rate is very high |

**Alert log:** `GET /api/alerts?limit=50` (newest first, `limit` 1–200):
```json
{ "count": 1, "alerts": [
  { "id": 19, "code": 4, "description": "the baby's breathing rate is very low", "ts": 1791712726,
    "details": {"bpm": 9}, "hasSnapshot": true, "snapshotUrl": "/api/alerts/19/snapshot" } ] }
```

**Clear the log:** `DELETE /api/alerts?before=1791712726` deletes alerts (and their snapshots) at or before `before` (unix seconds); without `before` it deletes all of them. Returns `{"message": "Alerts cleared", "removed": 3}`, 400 if `before` isn't an integer, 503 if the database is unreachable.

**Snapshot image:** `GET /api/alerts/<id>/snapshot` returns the JPEG (404 if none). An `<img src>` can't send the token, so fetch it and use a blob URL:
```ts
const res = await fetch(`${API}${alert.snapshotUrl}`, { headers: { Authorization: `Bearer ${token}` } });
const imgSrc = URL.createObjectURL(await res.blob());
```

**Live:** Socket.IO event `emergency_alert` on the same socket as `vitals`:
```json
{ "dbId": 19, "code": 4, "description": "the baby's breathing rate is very low", "ts": 1791712726,
  "details": {"bpm": 9}, "snapshotUrl": "/api/alerts/19/snapshot", "image": "data:image/jpeg;base64,...",
  "id": "alert-1791712726", "type": "4", "emergencyType": "4", "snapshot": "data:image/jpeg;base64,..." }
```
`image` can be shown directly (`<img src={alert.image}>`). Load the log once from `GET /api/alerts`, then add each pushed alert to the top; **`dbId` in the event is the same as `id` in the log**, so skip it if it's already in the list. `dbId` is `null` if saving to the database failed.

## Going live with Tiger Data
1. In your `backend/.env` (see above), set `TIGER_DATABASE_URL` and `TIGER_TABLE=infant_vitals`.
2. `python scripts/check_tiger.py` should print real readings.
3. In `app/data/__init__.py`, comment out the dummy import and uncomment the tiger import, then restart the server.
