import { BASE_URL, del, get, getAccessToken } from './client'

// Alert shapes from the backend (backend/app/data/format.py, alerts/dispatcher.py):
// - log entry (GET /alerts): { id, code, description, ts, details, hasSnapshot, snapshotUrl }
// - live "emergency_alert" event: the same fields with dbId instead of id, plus
//   image (the snapshot as a data URL).
// Both become one shape here, with ts in milliseconds like readings:
// { id, code, description, ts, details, hasSnapshot, image }.
// A live alert the backend couldn't save has dbId null; it gets a local id.
export function normalizeAlert(raw) {
  const live = 'dbId' in raw
  const id = live ? (raw.dbId ?? `live-${raw.ts}`) : raw.id
  return {
    id,
    code: raw.code ?? null,
    description: raw.description || 'Alert from the crib',
    ts: raw.ts * 1000,
    details: raw.details && typeof raw.details === 'object' ? raw.details : {},
    hasSnapshot: live ? Boolean(raw.image) : Boolean(raw.hasSnapshot),
    image: live && raw.image ? raw.image : null,
  }
}

// What an alert is about, for its icon and color. The board sends 101 (no
// breathing), 102 (baby temperature), 103 (room temperature) and 104 (baby
// out of the crib); 1-5 are earlier test codes (1-3 temperature, 4-5 breathing).
const KINDS = { 101: 'breath', 102: 'temp', 103: 'temp', 104: 'weight' }

export function alertKind(code) {
  if (KINDS[code]) return KINDS[code]
  if (code >= 1 && code <= 3) return 'temp'
  if (code >= 4 && code <= 5) return 'breath'
  return 'alert'
}

// "the baby's breathing rate is very low" -> "Breathing rate is very low"
export function alertTitle(description) {
  const text = description.replace(/^the baby's /i, '')
  return text.charAt(0).toUpperCase() + text.slice(1)
}

// The alert log, newest first.
export async function getAlerts(limit = 50) {
  const { alerts } = await get(`/alerts?limit=${limit}`)
  return alerts.map(normalizeAlert)
}

// Delete alerts at or before beforeMs (and their snapshots) from the log.
// Returns how many the backend removed.
export async function clearAlerts(beforeMs) {
  const { removed } = await del(`/alerts?before=${Math.floor(beforeMs / 1000)}`)
  return removed
}

// One alert's snapshot as an object URL for <img src>. The route needs the
// Auth0 token, which an <img> can't send, so it's fetched here. Call
// URL.revokeObjectURL on the result when done.
export async function getAlertSnapshot(id) {
  const token = await getAccessToken()
  const res = await fetch(`${BASE_URL}/alerts/${id}/snapshot`, {
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  })
  if (!res.ok) throw Object.assign(new Error('Snapshot unavailable'), { status: res.status })
  return URL.createObjectURL(await res.blob())
}
