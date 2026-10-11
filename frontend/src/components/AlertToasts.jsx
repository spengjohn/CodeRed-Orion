import { useEffect, useRef, useState } from 'react'
import { Link } from 'react-router'
import { alertKind, alertTitle } from '../api/alerts'
import { connectAlerts } from '../api/live'
import { useFormatters } from '../hooks/useFormatters'
import AlertIcon from './AlertIcon'

const MAX_QUEUED = 10

// Emergency alerts, shown on every page while signed in as a large card in the
// middle of the screen over a dimmed page. It stays until dismissed (it's an
// emergency). If more arrive, the newest shows first with a "+N more" note,
// and dismissing it reveals the next.
function AlertToasts() {
  const fmt = useFormatters()
  const [queue, setQueue] = useState([])
  const dismissButton = useRef(null)

  useEffect(
    () =>
      connectAlerts((alert) =>
        setQueue((list) => [alert, ...list.filter((a) => a.id !== alert.id)].slice(0, MAX_QUEUED)),
      ),
    [],
  )

  const current = queue[0]
  const dismiss = () => setQueue((list) => list.slice(1))

  // Focus the dismiss button when an alert opens, and let Escape dismiss it.
  useEffect(() => {
    if (!current) return undefined
    dismissButton.current?.focus()
    const onKey = (e) => {
      if (e.key === 'Escape') setQueue((list) => list.slice(1))
    }
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [current])

  if (!current) return null

  const kind = alertKind(current.code)
  const more = queue.length - 1

  return (
    <div className="alert-overlay">
      <div
        key={current.id}
        className="alert-modal"
        role="alertdialog"
        aria-modal="true"
        aria-labelledby="alert-modal-title"
        style={{ '--m': `var(--${kind})` }}
      >
        <div className="alert-modal-head">
          <AlertIcon kind={kind} />
          <span className="alert-modal-label">Alert · {fmt.time(current.ts)}</span>
        </div>

        <h2 id="alert-modal-title">{alertTitle(current.description)}</h2>
        {current.details.message && <p className="alert-modal-message">{current.details.message}</p>}
        {current.image && <img src={current.image} alt="Crib camera when the alert fired" />}

        <div className="alert-modal-actions">
          <button type="button" className="wbtn pri" ref={dismissButton} onClick={dismiss}>
            {more > 0 ? 'Dismiss' : 'Got it'}
          </button>
          <Link to="/alerts" className="wbtn gho" onClick={dismiss}>
            View alert log
          </Link>
        </div>
        {more > 0 && <p className="alert-modal-more">+{more} more alert{more > 1 ? 's' : ''}</p>}
      </div>
    </div>
  )
}

export default AlertToasts
