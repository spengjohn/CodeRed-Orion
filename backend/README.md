# Backend (Flask)

Serves crib readings to the frontend. Uses **dummy data** for now; Tiger Data comes later.

## Run
From `backend/`:
```
python -m venv venv
venv\Scripts\activate          # macOS/Linux: source venv/bin/activate
pip install -r requirements.txt
flask --app app run --debug    # http://localhost:5000
```

## Files
- `app/__init__.py`: creates the Flask app, turns on CORS, registers routes and Socket.IO.
- `app/data/__init__.py`: data source switch. Comment out the dummy import and uncomment the Tiger Data import to go live.
- `app/data/dummy.py`: generates realistic fake readings from the timestamp (same second gives the same values).
- `app/data/format.py`: `to_reading()`, the one place that defines the JSON shape below.
- `app/api/readings.py`: REST endpoints `/api/readings/latest` and `/api/readings/history`.
- `app/realtime/__init__.py`: pushes the latest reading to all connected clients every second.
- `scripts/watch_vitals.py`: terminal client that prints pushed readings (needs `pip install "python-socketio[client]"`).

## JSON for the frontend
One reading (`ts` = unix seconds, weight in kg, temperatures in °C):
```json
{ "ts": 1791663137, "babyWeightKg": 4.21, "ambientTempC": 23.0, "babyTempC": 36.9, "breathsPerMin": 39, "isMoving": false }
```

`GET /api/readings/latest` returns one reading.

`GET /api/readings/history?range=3600&bucket=60` (seconds; max range 7 days, max 2000 points):
```json
{ "range": 3600, "bucket": 60, "series": [ { "ts": 1791659340, "babyWeightKg": 4.2, "ambientTempC": 22.1, "babyTempC": 37.0, "breathsPerMin": 36, "isMoving": false } ] }
```
Each series point is the average over its bucket; `ts` is the start of the bucket. `isMoving` is true if the baby moved at any point in the bucket.

## Live push (Socket.IO)
The backend sends a `vitals` event with one reading every second, so there is no need to poll.
```ts
import { io } from "socket.io-client";
const socket = io("http://localhost:5000");
socket.on("vitals", (reading) => { /* same shape as /latest */ });
```
Load charts once from `/history`, then append each pushed reading. Ignore a reading if its `ts` isn't newer than the last one.
