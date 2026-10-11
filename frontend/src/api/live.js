import { io } from 'socket.io-client'
import { normalizeAlert } from './alerts'
import { getAccessToken } from './client'
import { normalizeReading } from './readings'

// Path the backend's Socket.IO server listens on. Same origin as the page;
// the Vite proxy (dev) and nginx (production) forward it to Flask.
const SOCKET_PATH = '/socket.io'

// One connection shared by every subscriber (vitals and video), opened on the
// first subscribe and closed after the last unsubscribe. The backend rejects
// connections without an Auth0 token; the auth callback runs on every
// (re)connect, so a refreshed token is picked up automatically.
let socket = null
let users = 0

function acquire() {
  if (!socket) {
    socket = io({
      path: SOCKET_PATH,
      auth: (cb) => getAccessToken().then((token) => cb(token ? { token } : {})),
    })
  }
  users += 1
  return socket
}

function release() {
  users -= 1
  if (users === 0) {
    socket.disconnect()
    socket = null
  }
}

// Attach { event: handler } to the shared socket; returns an unsubscribe function.
function subscribe(handlers) {
  const s = acquire()
  for (const [event, fn] of Object.entries(handlers)) s.on(event, fn)
  return () => {
    for (const [event, fn] of Object.entries(handlers)) s.off(event, fn)
    release()
  }
}

// Subscribe to the backend's live "vitals" event (the newest reading, pushed
// every second). onStatus(true|false) reports when the connection opens, drops
// or is refused; Socket.IO reconnects on its own. Returns an unsubscribe function.
export function connectLive(onReading, onStatus = () => {}) {
  const unsubscribe = subscribe({
    vitals: (reading) => onReading(normalizeReading(reading)),
    connect: () => onStatus(true),
    disconnect: () => onStatus(false),
    connect_error: () => onStatus(false),
  })
  if (socket.connected) onStatus(true)
  return unsubscribe
}

// Subscribe to camera frames: "video_frame" { ts, frame (JPEG data URL), frameId },
// relayed from the crib's edge device at up to ~12 per second.
export function connectFrames(onFrame) {
  return subscribe({ video_frame: onFrame })
}

// Subscribe to emergency alerts from the crib ("emergency_alert"), normalized
// by api/alerts.js. Returns an unsubscribe function.
export function connectAlerts(onAlert) {
  return subscribe({ emergency_alert: (alert) => onAlert(normalizeAlert(alert)) })
}
