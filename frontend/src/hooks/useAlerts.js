import { useCallback, useEffect, useState } from 'react'
import { clearAlerts, getAlerts } from '../api/alerts'
import { connectAlerts } from '../api/live'

// Add alerts to a newest-first list, skipping ones already in it (a live alert
// can also arrive in the log loaded over REST: same id).
export function mergeAlerts(list, incoming, max = 200) {
  const seen = new Set(list.map((a) => a.id))
  const added = incoming.filter((a) => !seen.has(a.id))
  return [...added, ...list].sort((a, b) => b.ts - a.ts).slice(0, max)
}

// The alert log: loads recent alerts over REST, then adds new ones pushed
// live over Socket.IO, newest first.
export function useAlerts({ limit = 50 } = {}) {
  const [alerts, setAlerts] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)

  useEffect(() => {
    let cancelled = false
    getAlerts(limit)
      .then((log) => !cancelled && setAlerts((live) => mergeAlerts(live, log)))
      .catch((err) => !cancelled && setError(err))
      .finally(() => !cancelled && setLoading(false))

    const close = connectAlerts((alert) => setAlerts((list) => mergeAlerts(list, [alert])))
    return () => {
      cancelled = true
      close()
    }
  }, [limit])

  // Delete the listed alerts from the log. Only those up to the newest one
  // shown, so a live alert arriving meanwhile isn't removed unseen. Throws on failure.
  const clear = useCallback(async () => {
    if (alerts.length === 0) return
    const newest = alerts[0].ts
    await clearAlerts(newest)
    setAlerts((list) => list.filter((a) => a.ts > newest))
  }, [alerts])

  return { alerts, loading, error, clear }
}
