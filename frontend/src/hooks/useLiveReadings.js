import { useEffect, useState } from 'react'
import { getHistory } from '../api/readings'
import { connectLive } from '../api/live'

const TICK_MS = 5000

// Dashboard data: loads the last hour of history over REST, then appends new
// readings pushed over Socket.IO.
//
// The board stores one snapshot per minute and the backend re-pushes the
// newest one every second, so two separate states are reported:
// - isConnected: the Socket.IO connection is up.
// - isStale: the newest reading is older than staleAfterMs (by its own ts),
//   meaning the board has stopped reporting even if the connection is fine.
export function useLiveReadings({
  historyS = 60 * 60,
  bucketS = 60,
  maxPoints = 2000,
  staleAfterMs = 3 * 60 * 1000,
} = {}) {
  const [readings, setReadings] = useState([])
  const [loaded, setLoaded] = useState(false)
  const [isConnected, setIsConnected] = useState(false)
  const [now, setNow] = useState(() => Date.now())
  const [error, setError] = useState(null)

  useEffect(() => {
    let cancelled = false

    getHistory(historyS, bucketS)
      .then((history) => {
        if (cancelled) return
        // Keep any live readings that arrived while history was loading.
        setReadings((live) => {
          const lastHistoric = history.at(-1)?.ts ?? -Infinity
          return [...history, ...live.filter((r) => r.ts > lastHistoric)].slice(-maxPoints)
        })
      })
      .catch((err) => !cancelled && setError(err))
      .finally(() => !cancelled && setLoaded(true))

    const close = connectLive(
      (reading) => {
        // The same row is pushed many times; only keep newer readings.
        setReadings((prev) =>
          prev.length && reading.ts <= prev.at(-1).ts ? prev : [...prev, reading].slice(-maxPoints),
        )
      },
      (connected) => !cancelled && setIsConnected(connected),
    )

    const tick = setInterval(() => setNow(Date.now()), TICK_MS)

    return () => {
      cancelled = true
      close()
      clearInterval(tick)
    }
  }, [historyS, bucketS, maxPoints])

  const latest = readings.at(-1) ?? null
  // Weight is unreliable while the child moves, so show the last still reading.
  const latestStill = readings.findLast((r) => !r.isMoving) ?? null

  return {
    readings,
    latest,
    latestStill,
    hasData: readings.length > 0,
    // Wait for the first history load so the dashboard doesn't flash offline.
    isStale: loaded && (!latest || now - latest.ts > staleAfterMs),
    isConnected,
    error,
    // Updates every few seconds, for "2 minutes ago" labels.
    now,
  }
}
