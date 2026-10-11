# Whisker Frontend

Whisker is a smart baby monitoring system for cribs. Sensors in the crib report the baby's temperature, weight, breaths per minute, and movement, and parents view those readings on this website (and on a mobile app if time allows).

Built for the CodeRED Orion hackathon with React + Vite (JavaScript). The backend is the Flask app in [`../backend`](../backend).

## Getting Started

The frontend needs the Flask backend running for any real data. Start it in one terminal (see [`backend/README.md`](../backend/README.md) for the virtualenv setup):

```bash
cd backend
flask --app app run      # http://localhost:5000, serves dummy data for now
```

Then in a second terminal:

```bash
cd frontend
cp .env.example .env     # local hosts/ports (gitignored)
npm install              # install dependencies
npm run dev              # dev server with hot reload; proxies /api and /socket.io to Flask
npm test                 # unit tests (no backend needed)
npm run test:contract    # API contract tests (backend must be running)
npm run lint             # lint with oxlint
npm run build            # production build to dist/
npm run preview          # serve the production build locally
```

### Environment (`.env`)

| Variable | Default | Used by |
|---|---|---|
| `BACKEND_URL` | unset | Full backend URL, e.g. `https://whisker.health`; wins over host and port. Use it to develop against the deployed backend, which is only reachable through nginx over HTTPS |
| `BACKEND_HOST` | `localhost` | Vite proxy target, contract tests |
| `BACKEND_PORT` | `5000` | Same; must match Flask's port (`flask --app app run` uses 5000; `python run.py` reads `PORT` from `backend/.env`) |
| `FRONTEND_HOST` | `localhost` | `npm run dev` and `npm run preview` |
| `FRONTEND_PORT` | `5173` | Same; fails instead of picking another port if it's taken |
| `VITE_API_BASE_URL` | `/api` | Optional. REST base URL baked into the built site (`src/api/client.js`); leave unset so requests go through the Vite proxy or nginx |
| `VITE_AUTH0_DOMAIN`, `VITE_AUTH0_CLIENT_ID` | unset | Auth0 sign-in (`src/auth/AuthProvider.jsx`). Unset = demo mode, see [Auth0 setup](#auth0-setup) |
| `VITE_AUTH0_AUDIENCE` | unset | API audience for access tokens; must equal `AUTH0_AUDIENCE` in `backend/.env` |
| `DEV_AUTH_TOKEN` | unset | An Auth0 access token the dev proxy adds to `/api` and `/socket.io` requests; also used by the contract tests. Never `VITE_`-prefixed. See [Dev shortcut](#dev-shortcut-dev_auth_token) |
| `AUTH_TOKEN` | unset | Contract tests only; overrides `DEV_AUTH_TOKEN` |
| `DEV_PROXY_LOG` | on | `npm run dev` logs each proxied backend call; `0` turns it off |

Only `VITE_`-prefixed variables reach the built site; the others are read by `vite.config.js` and the contract tests only. Without a `.env` the defaults apply. Contract tests still prefer `API_URL` when it's set.

### Auth0 setup

The backend only accepts Socket.IO connections (live readings and video) with an Auth0 access token, and will require one on the readings REST routes too (Backend Request 3). To sign in for real:

1. In the Auth0 dashboard, create a **Single Page Application**. Add `http://localhost:5173` and `https://whisker.health` to **Allowed Callback URLs**, **Allowed Logout URLs** and **Allowed Web Origins**, and turn on **Refresh Token Rotation**.
2. Put its domain and client ID in `frontend/.env` as `VITE_AUTH0_DOMAIN` and `VITE_AUTH0_CLIENT_ID`, and set `VITE_AUTH0_AUDIENCE` to the backend API's identifier (the same value as `AUTH0_AUDIENCE` in `backend/.env`).
3. Running Flask locally? The dev proxy strips the `Origin` header from Socket.IO requests, so its `FRONTEND_URL` origin check doesn't get in the way in dev. Anything connecting directly needs its origin in `FRONTEND_URL` (Backend Request 9).

Without the `VITE_AUTH0_*` values the app runs in **demo mode**: **Sign in** just sets a local flag and shows a demo user, and the browser has no token of its own.

#### Dev shortcut: `DEV_AUTH_TOKEN`

Put an Auth0 access token for the backend's API in `.env` as `DEV_AUTH_TOKEN` (for example from Auth0's API **Test** tab). `npm run dev` then adds it as `Authorization: Bearer ...` to every proxied `/api` and `/socket.io` request that doesn't already carry one, so demo mode gets real data. A real Auth0 login still uses its own token. The token stays in the dev server: it's never sent to the browser or built into `dist/`.

On startup the dev server prints `Dev proxy → <backend> (DEV_AUTH_TOKEN attached)` and warns when the token has expired; paste a fresh one then. While you test, it also logs every backend call it forwards, for example `21:04:31.218  GET  /api/readings/history?range=3600&bucket=60  → 200  142 ms  [dev token]`, plus WebSocket opens and closes (Socket.IO events inside an open connection aren't shown). Set `DEV_PROXY_LOG=0` to turn this off. To develop against production, add `BACKEND_URL=https://whisker.health`.

If Flask isn't running, pages show errors like `Couldn't load readings: GET /readings/history?... failed: 502` and the dashboard shows "Disconnected" or "Sensors offline".

## Deployment Plan

1. **Run and build locally first.** Check the app with `npm run dev` against Flask, run `npm test` and `npm run test:contract`, then run `npm run build` and confirm `npm run preview` looks correct.
2. **Deploy to the AWS EC2 instance.** Copy the built `dist/` folder to the server (the backend expects it at `/var/www/whisker-frontend`) and serve it with nginx at `https://whisker.health`. The server does not need Node or `node_modules`, only the static files.

nginx notes:
- The app uses client-side routing, so nginx must fall back to `index.html` for unknown paths (`try_files $uri /index.html;`).
- Proxy `/api/` to Flask on port 5000.
- Also proxy `/socket.io/` to Flask **with WebSocket upgrade headers**; live readings use Socket.IO on that path, outside `/api` (see [Backend Requests](#backend-requests)).
- Serve over HTTPS; Auth0 sign-in only works on secure origins (and localhost).
- `VITE_`-prefixed environment variables are baked in at build time, so set them before building and never put secrets in them.

## Data Contract

The source of truth is [`backend/README.md`](../backend/README.md) and `to_reading()` in `backend/app/data/format.py`. Summary:

```json
{ "ts": 1791663137, "babyWeightKg": 4.21, "ambientTempC": 23.0, "babyTempC": 36.9, "breathsPerMin": 39, "isMoving": false }
```

| Field | Type | Notes |
|---|---|---|
| `ts` | integer | Unix time in **seconds**, UTC. Converted to milliseconds once on arrival (`normalizeReading`); everywhere in the frontend `ts` is ms |
| `babyWeightKg` | number | Weight, kg (2 decimals) |
| `ambientTempC` | number | Room temperature, °C (1 decimal) |
| `babyTempC` | number | Baby temperature, °C (1 decimal) |
| `breathsPerMin` | number | Breathing rate (whole number) |
| `isMoving` | boolean | The board detected movement **at some point during that minute**. In history buckets: true if any minute in the bucket had movement |

**Data rate:** the crib board stores **one snapshot per minute** (Tiger Data). Each row stands for the minute starting at its `ts`, so the frontend never requests buckets under 60 s and treats each moving row as up to one minute of movement (`SNAPSHOT_INTERVAL_MS` in `src/hooks/useReadings.js`). The current dummy data is generated per second; see [Backend Requests](#backend-requests).

The backend always sends metric units and UTC. All unit conversion and time-zone handling happens on the frontend.

### Endpoints
| Endpoint | Used by |
|---|---|
| `GET /api/readings/history?range=<s>&bucket=<s>` → `{ range, bucket, series }` | Analytics pages, dashboard's initial hour. Relative to now; each point is averaged over its bucket and `ts` is the bucket start. Max range 30 days, max 2000 points |
| `GET /api/readings/latest` → one reading | Available; `getLatest()` in `src/api/readings.js` |
| All REST calls | Send `Authorization: Bearer <token>` when signed in with Auth0 (`src/api/client.js`); a 401 shows "Your session has expired. Sign in again." |
| Socket.IO event `vitals` (path `/socket.io`) → newest reading, pushed every second | Dashboard live updates. With minute snapshots the same row repeats ~60 times; the frontend keeps only readings with a newer `ts`. The connection needs an Auth0 token (`auth: { token }`) |
| `GET /api/health` | Contract tests check the backend is up |
| `GET /api/public`, `GET /api/private-crib-data` | Auth0 test routes (the second needs a Bearer token, else 401). Not used by the frontend yet; contract tests check they're served |
| Socket.IO event `video_frame` → `{ ts, frame, frameId }`, `frame` a `data:image/jpeg;base64,...` URL, up to ~12 per second | Dashboard camera tile; see [Live Feed](#live-feed) |
| `GET /api/video/status` → `{ streaming_active, target_fps }`; `POST /api/video/start`, `/api/video/stop` (Bearer token) | Not used by the frontend yet; contract tests check status and that stop needs a token |

Range picker → request (`src/hooks/useReadings.js`):

| Range | `range` | `bucket` | Points |
|---|---|---|---|
| 1h | 3600 | 60 | 60 (raw minute snapshots) |
| 24h | 86400 | 300 | 288 |
| 7d | 604800 | 1800 | 336 |
| 14d | 1209600 | 3600 | 336 |
| 30d | 2592000 | 7200 | 360 (the backend's maximum range) |

## Frontend Responsibilities

| Area | Where |
|---|---|
| REST calls, seconds → ms conversion | `src/api/readings.js` (`getHistory`, `getLatest`, `normalizeReading`) |
| Live readings and camera frames over one shared, token-authenticated Socket.IO connection (reconnects automatically) | `src/api/live.js` |
| Camera tile: newest frame, Live while frames arrive (5 s timeout) | `src/hooks/useCribFrames.js`, `src/components/CribVideo.jsx` |
| °C/°F, kg/lb/lb+oz conversion and display formatting | `src/utils/units.js` |
| Time formatting, 12h/24h, device time zone, "2 minutes ago", durations like "1 h 5 min" (built-in `Intl`, no date library) | `src/utils/time.js` |
| Chart downsampling, min/avg/max stats, movement spans, activity series and stats | `src/utils/series.js` |
| Preference-aware formatters for components | `src/hooks/useFormatters.js` |
| History by range (1h/24h/7d/14d/30d) | `src/hooks/useReadings.js` |
| Dashboard live data and duplicate-reading filtering. Status: **Disconnected** when Socket.IO is down; **Sensors offline** when the newest reading's `ts` is more than 3 minutes old, **Camera offline** when no video frame has arrived for 5 seconds, **Monitor offline** when both (works even though the backend keeps re-pushing the last row) | `src/hooks/useLiveReadings.js` |
| Activity chart data and stats by range | `src/hooks/useActivity.js` |
| Charts (Recharts), themed from the tokens | `src/components/MetricChart.jsx`, `AnalyticsView.jsx`, `ActivityChart.jsx`, `chartStyle.js`, `src/hooks/useThemeColors.js` |
| Dashboard metric cards | `src/components/MetricCard.jsx` |
| Sign-in (Auth0, or demo mode without config), access tokens for REST and Socket.IO, route protection, header menu | `src/auth/`, `src/api/client.js`, `src/components/UserMenu.jsx` |
| Change since 1 hr / this week, time since moving | `change` and `currentActivity` in `src/utils/series.js` |

**Movement (`isMoving`):** shown as a status and an Activity button on the dashboard, charted on its own Activity page, and as shaded areas on the other charts. Weight readings are unreliable while the child moves, so the dashboard shows the last still reading and weight stats ignore moving readings (falling back to all readings if every bucket is marked moving).

**Backend responsibilities:** storing readings, bucketing and averaging history, pushing live readings, auth, and the video feed.

## Design

The look comes from [`whisker-style-tile.html`](whisker-style-tile.html): open it in a browser to see the palette, fonts, and dashboard cards in light and dark, and to tweak them. The app is designed **desktop first**; a proper mobile layout comes later.

- **Tokens** live in `src/tokens.css` (colors, `--font-display`, `--font-number`, `--roundness`). The names match the tile's **Copy CSS** output, so after changing the tile, paste the exported values over the ones in `tokens.css` (keep the dark values in both dark blocks).
- **Dark mode** follows the system until the Dark mode switch in settings is flipped, which saves light or dark and sets `<html data-theme="light">` or `"dark"`. `index.html` applies the saved choice before first paint.
- **Fonts**: Nunito (headings and numbers) and Nunito Sans (text), bundled with `@fontsource` so there's no request to Google Fonts.
- **Component styles** are in `src/App.css`, using the tile's class names (`.metric`, `.m-val`, `.alert`, `.video`, `.wbtn`...). Corner radii scale with `--roundness`.
- **Charts** can't use CSS variables directly (Recharts writes SVG attributes), so they read token colors through `useThemeColors()`. Series colors are token names, e.g. `color: 'temp'`.

## User Preferences

Chosen on the User settings page and saved in the browser's localStorage (`whisker.preferences`):

| Preference | Options | Default |
|---|---|---|
| Temperature | °C, °F | °C |
| Weight | kg, lb, lb + oz | kg |
| Time format | 12-hour, 24-hour | 12-hour |
| Dark mode | On, Off (switch) | Follows the system until first flipped |

Times are always shown in the device's own time zone; it isn't a preference.

Components read them through `usePreferences()` (`src/preferences/context.js`) or, for formatting, `useFormatters()`.

## Testing

Tests use **Jest**, with **Supertest** for the API contract.

```bash
npm test               # unit tests: utils, hooks, preferences (jsdom), no backend needed
npm run test:contract  # contract tests against the running backend
```

The contract tests (`tests/contract/`) check that the backend still sends what the frontend expects: reading fields and types, `ts` in seconds, history envelope for every range the frontend requests, ordering and bucket alignment, 400 errors for bad parameters, that Socket.IO refuses connections without a token and (with `AUTH_TOKEN`) pushes valid `vitals`, the video status route, and that the Auth0 routes are served (public → 200, private without a token → 401). They use `DEV_AUTH_TOKEN` (or `AUTH_TOKEN`) from `.env` for the authenticated checks; without one the `vitals` test is skipped. The "readings reject requests without a token" test fails on purpose until Backend Request 3 lands. They target `BACKEND_HOST:BACKEND_PORT` from `.env` (`http://localhost:5000` by default) and fail with a clear message if the backend isn't running. To test a deployed backend:

```powershell
$env:API_URL="https://whisker.health"; npm run test:contract   # PowerShell
```
```bash
API_URL=https://whisker.health npm run test:contract           # bash
```

Supertest doesn't support Socket.IO, so the live test uses `socket.io-client` directly. The expected reading shape lives in `tests/contract/packetSchema.js`; update it together with the frontend if the backend changes `to_reading()`.

## Backend Requests

Open backend changes the frontend needs:

1. **Fix Socket.IO through nginx (blocking production).** On `https://whisker.health`, every `/socket.io/` request (e.g. `/socket.io/?EIO=4&transport=polling`) returns **500 from nginx** itself, with or without an `Origin` header, while `/api/*` works. Add a `location /socket.io/` that proxies to Flask with `proxy_http_version 1.1;`, `proxy_set_header Upgrade $http_upgrade;`, `proxy_set_header Connection "upgrade";` and `proxy_set_header Host $host;`, then check nginx's error log for the current 500. Alternatively start Socket.IO with `path="/api/socket.io"` (then change `SOCKET_PATH` in `src/api/live.js`). Unblocks: live readings and video in production and when developing against it. Separately, `http://18.223.70.95/` serves nginx's default site (404) and `https://18.223.70.95/` returns 502; fine if only the domain is meant to work.
2. **Add a movement fraction to history buckets**, e.g. `movingFraction` = fraction of minutes in the bucket with movement (average of `is_moving`, 0–1), alongside `isMoving` in `backend/app/data/dummy.py` and the future Tiger Data query. Currently a bucket is "moving" if any minute in it had movement, so on long ranges nearly every bucket is marked moving. Unblocks: meaningful activity % and still-only weight stats on 24h+ ranges.
3. **Make the readings REST routes private (top priority).** Socket.IO now requires an Auth0 token, but `/api/readings/latest` and `/api/readings/history` are still public, so anyone can read the baby's vitals over REST. Add `@require_auth` (from `app.auth.auth`) under `@bp.get("/latest")` and `@bp.get("/history")` in `backend/app/api/readings.py`, the same way `private_crib_data` does. The frontend already sends `Authorization: Bearer <token>` on every request, so nothing else changes. Keep `/api/health` and `/api/public` open; consider protecting `/api/video/status` too.
4. **Make dummy data match the real data rate.** `get_history` and `get_latest` in `backend/app/data/dummy.py` generate a reading for every second, and `_is_moving` marks 10 s of every 4 minutes. Real data is one row per minute with `is_moving` meaning "moved during that minute". Generating minute rows (e.g. `ts` floored to the minute) would make dev charts, live updates, and activity stats behave like production.
5. **Push only when there's a new row** (optional). `backend/app/realtime/__init__.py` emits `get_latest()` every second, which will be the same minute row ~60 times. Emitting only when a new row lands would cut traffic; the frontend already ignores repeats, so this isn't blocking.
6. **Reject `bucket` below 60 s** in `backend/app/api/readings.py` (or document it). With one row per minute, sub-minute buckets will come back sparse or empty once Tiger Data is live.
7. **Update the backend docs.** `MAX_RANGE_S` in `backend/app/api/readings.py` is now 30 days, but its comment and `backend/README.md` ("max range 7 days") still say 7; the frontend's 14d and 30d ranges rely on the 30-day cap. `backend/README.md` also doesn't mention that Socket.IO requires a token (its sample `io("http://localhost:5000")` no longer connects), the `video_frame` event, `/api/video/*`, or the `FRONTEND_URL` and `EDGE_SECRET` settings.
8. ~~Read the Flask port from the environment.~~ Done: `backend/run.py` reads `HOST`, `PORT` and `DEBUG`. `flask --app app run`, which both READMEs use, still ignores `PORT`; either document `python run.py` or keep 5000.
9. **Allow the dev origin for Socket.IO.** `create_app()` passes `cors_allowed_origins=[FRONTEND_URL]`, defaulting to `http://localhost:1234`. Vite runs on 5173 and its proxy forwards the browser's `Origin`, so the handshake is refused even with a valid token. Default to `http://localhost:5173`, or accept a comma-separated list so dev and `https://whisker.health` both work.
10. **Send `vitals` to the first client right away** (minor). In `on_connect` (`backend/app/realtime/__init__.py`), the first client starts the push loop and `return`s before `emit("vitals", ...)`, so it waits up to a second for its first reading.
11. **Only send video to clients that want it** (optional). `video_frame` is broadcast to every socket at up to 12 FPS, including pages that don't show video. A Socket.IO room joined by the dashboard would cut traffic.
12. **Require `EDGE_SECRET` in production.** `get_edge_secret()` in `backend/app/realtime/video_relay.py` falls back to `"hackathon-secret"`, so if it isn't set, anyone who can reach port 8765 can inject frames.

## Known Issues

- **Activity on long ranges:** history buckets are marked moving if any movement occurred, so on 24h+ ranges the activity chart is mostly 100%, activity % is inflated, and weight stats fall back to using all readings (Backend Request 2).
- **Dummy data is per-second:** until Backend Request 4, dev data has a new reading every second, so the dashboard's "Last reading" and 1h charts update faster and look smoother than real minute snapshots will.
- **Clock-based offline check:** "Sensors offline" compares the reading's `ts` to this device's clock, so a device clock more than a few minutes off could show it wrongly.
- **Video is JPEG frames, not WebRTC:** ~12 base64 frames a second over Socket.IO uses more bandwidth than a video codec and has no audio; fine for the demo.
- **Demo mode needs `DEV_AUTH_TOKEN` for data:** without `VITE_AUTH0_*` the browser has no token, so the backend refuses Socket.IO, and once Backend Request 3 lands, REST too. `DEV_AUTH_TOKEN` fixes this in `npm run dev` only; a built site needs Auth0 ([Auth0 setup](#auth0-setup)).
- **No live data from production yet:** whisker.health's nginx returns 500 for Socket.IO (Backend Request 1), so against it the dashboard loads history but shows Disconnected and Camera offline.
- **Readings REST is still public** on the backend until Backend Request 3.
- **Desktop first:** below 900px the dashboard simply stacks; there's no designed mobile layout yet.
- **Settings are browser-only:** `src/api/settings.js` has no backend endpoint and is unused; preferences live in localStorage per device.
- **Contract tests need the backend running**, either locally at `BACKEND_HOST:BACKEND_PORT` from `.env`, or at `BACKEND_URL` / `API_URL`. `npm test` doesn't.
- **`npm audit`** reports moderate vulnerabilities in dev-only test packages (Jest/jsdom/Babel); they aren't part of the built site.

## Pages

### Header
Only the Whisker logo (to the dashboard when signed in, the splash when signed out). Signed-in users get a hamburger menu in the top right with their name and email, **User settings**, and **Log out** (`src/components/UserMenu.jsx`). Pages are reached from the dashboard rather than header links.

### Splash / Landing Page
What signed-out visitors see: a short blurb about what Whisker monitors, with a **Sign in** button at the bottom. There's no sign-in page; **Sign in** goes to Auth0's hosted login and returns here (or signs in a demo user without Auth0 config, see [Auth0 setup](#auth0-setup)). Signed-in users who visit `/` go straight to the dashboard, and every other page sends signed-out visitors back here (`src/auth/RequireAuth.jsx`).

### Home / Dashboard
At-a-glance view of the baby's current status:
- **Live feed** of the crib camera (see [Live Feed](#live-feed) below)
- **Layout** (from the style tile): the live feed on the left and a 2×2 grid of metric cards on the right
- **Four metric cards**, each showing the current reading and a change line, and linking to its analytics page:
  - Temp: baby temperature, change since 1 hr, and room temperature → Temperature analytics
  - Activity: moving or still, and how long (e.g. "3 min since moving") → Activity analytics
  - Weight: last still reading and change this week (from 7d history) → Weight analytics
  - Breathing: breaths per minute and change since 1 hr → Breathing analytics
- **Status**: the badge on the video shows Live while camera frames are arriving, else Offline; an alert banner appears for Sensors offline (newest reading over 3 minutes old), Camera offline (no video frame for 5 seconds), Monitor offline (both) or Disconnected (no connection to the backend); the header shows how long ago the last reading was taken

### Analytics Pages
One page per metric, each with a back arrow to the dashboard beside the heading, a 1h / 24h / 7d / 14d / 30d range picker, a chart with movement shaded, and min/avg/max for the range:
- **Temperature**: baby temperature and ambient temperature
- **Weight** (stats ignore readings taken while moving)
- **Breathing**: breaths per minute
- **Activity**: when the child was registered as moving (`isMoving`), as a step chart per time bucket, with stats for number of movement episodes, total time moving, longest episode, and % of time moving (see Known Issues for long ranges)

### User Settings
Reached from the header menu (`/settings`). One card in two columns that stack on narrow screens. The first column is **Account** (the signed-in user's name and email, from Auth0 or the demo user) with **Appearance** (the Dark mode switch) always under it; the second is **Units & time**, which holds temperature, weight and time format (see [User Preferences](#user-preferences)). Device settings to come.

## Live Feed

The camera at the crib sends JPEG frames to the backend's edge relay (TCP port 8765, `backend/app/realtime/video_relay.py`), which forwards them to browsers as the Socket.IO `video_frame` event at up to ~12 per second: `{ ts, frame, frameId }`, where `frame` is a JPEG data URL (`data:image/jpeg;base64,...`, built by `to_video_frame` in `backend/app/data/format.py`).

- `src/api/live.js` shares one authenticated Socket.IO connection between live readings and frames.
- `src/hooks/useCribFrames.js` keeps only the newest frame as a `data:image/jpeg;base64,...` URL (bare base64 is prefixed, so either shape works). The feed is **Live** while frames keep arriving and goes **Offline** 5 s after the last one (timed by arrival, so the edge device's clock doesn't matter).
- `src/components/CribVideo.jsx` draws it as an `<img>` filling the video tile. It's a separate component so a dozen updates a second re-render only the tile, not the dashboard.

To try it locally, with the backend running and signed in through Auth0:

```bash
cd backend
python scripts/simulate_edge_camera.py   # synthetic frames; Pillow draws a test image, else a tiny placeholder
```

The relay is on by default; `POST /api/video/stop` and `/api/video/start` (Bearer token) pause and resume forwarding. If video later needs audio or lower bandwidth, WebRTC (or a managed service such as AWS Kinesis Video Streams or LiveKit) would replace the frame stream.
