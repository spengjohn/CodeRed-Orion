import { useEffect, useState } from 'react'
import { getAlertSnapshot } from '../api/alerts'

// An alert's snapshot as an <img> src: the live alert's own image if it has
// one, otherwise fetched from the backend (with the login token) when `load`
// is true. Returns { src, failed }.
export function useAlertSnapshot(alert, load = true) {
  const [fetched, setFetched] = useState({ id: null, src: null, failed: false })

  const needsFetch = load && alert.hasSnapshot && !alert.image && typeof alert.id === 'number'

  useEffect(() => {
    if (!needsFetch) return undefined
    let url = null
    let cancelled = false
    getAlertSnapshot(alert.id)
      .then((objectUrl) => {
        url = objectUrl
        if (!cancelled) setFetched({ id: alert.id, src: objectUrl, failed: false })
      })
      .catch(() => !cancelled && setFetched({ id: alert.id, src: null, failed: true }))
    return () => {
      cancelled = true
      if (url) URL.revokeObjectURL(url)
    }
  }, [alert.id, needsFetch])

  if (alert.image) return { src: alert.image, failed: false }
  if (fetched.id !== alert.id) return { src: null, failed: false }
  return { src: fetched.src, failed: fetched.failed }
}
