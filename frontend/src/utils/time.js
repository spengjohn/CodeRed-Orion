// Timestamp formatting with the browser's built-in Intl APIs. All functions
// take milliseconds and a { timeZone, timeFormat } object. timeZone is optional
// and defaults to the device's zone; the app always leaves it out.

const DAY_MS = 24 * 60 * 60 * 1000

const hourCycle = (timeFormat) => (timeFormat === '24h' ? 'h23' : 'h12')

export function formatTime(ms, { timeZone, timeFormat }) {
  return new Intl.DateTimeFormat('en-US', {
    timeZone,
    hour: 'numeric',
    minute: '2-digit',
    hourCycle: hourCycle(timeFormat),
  }).format(ms)
}

export function formatDate(ms, { timeZone }) {
  return new Intl.DateTimeFormat('en-US', {
    timeZone,
    month: 'short',
    day: 'numeric',
  }).format(ms)
}

export function formatDateTime(ms, prefs) {
  return `${formatDate(ms, prefs)}, ${formatTime(ms, prefs)}`
}

// Chart X axis labels: times for ranges up to a day, dates beyond that.
export function formatAxisTick(ms, rangeMs, prefs) {
  return rangeMs <= DAY_MS ? formatTime(ms, prefs) : formatDate(ms, prefs)
}

const relative = new Intl.RelativeTimeFormat('en', { numeric: 'auto' })

// "just now", "2 minutes ago", "3 hours ago", "yesterday"...
export function formatRelative(ms, now = Date.now()) {
  const seconds = Math.round((now - ms) / 1000)
  if (seconds < 10) return 'just now'
  if (seconds < 60) return relative.format(-seconds, 'second')
  if (seconds < 3600) return relative.format(-Math.floor(seconds / 60), 'minute')
  if (seconds < 86400) return relative.format(-Math.floor(seconds / 3600), 'hour')
  return relative.format(-Math.floor(seconds / 86400), 'day')
}

// Lengths of time: "45 s", "12 min", "1 h 5 min".
export function formatDuration(ms) {
  if (ms < 60 * 1000) return `${Math.round(ms / 1000)} s`
  const minutes = Math.round(ms / (60 * 1000))
  const hours = Math.floor(minutes / 60)
  return hours ? `${hours} h ${minutes % 60} min` : `${minutes} min`
}
