import { useEffect, useState } from 'react'
import { Link } from 'react-router'
import { alertKind, alertTitle } from '../api/alerts'
import { connectAlerts } from '../api/live'
import { useFormatters } from '../hooks/useFormatters'
import AlertIcon from './AlertIcon'

const MAX_TOASTS = 3

// Pop-ups for emergency alerts, shown on every page while signed in. Each one
// stays until dismissed (it's an emergency), newest on top.
function AlertToasts() {
  const fmt = useFormatters()
  const [toasts, setToasts] = useState([])

  useEffect(
    () =>
      connectAlerts((alert) =>
        setToasts((list) => [alert, ...list.filter((t) => t.id !== alert.id)].slice(0, MAX_TOASTS)),
      ),
    [],
  )

  const dismiss = (id) => setToasts((list) => list.filter((t) => t.id !== id))

  if (toasts.length === 0) return null

  return (
    <div className="alert-toasts">
      {toasts.map((alert) => {
        const kind = alertKind(alert.code)
        return (
          <div key={alert.id} className="alert-toast" role="alert" style={{ '--m': `var(--${kind})` }}>
            <AlertIcon kind={kind} />
            <div className="alert-toast-body">
              <span className="alert-toast-label">Alert · {fmt.time(alert.ts)}</span>
              <strong>{alertTitle(alert.description)}</strong>
              {alert.details.message && <span className="muted">{alert.details.message}</span>}
              {alert.image && <img src={alert.image} alt="Crib camera when the alert fired" />}
              <Link to="/alerts" className="alert-toast-link" onClick={() => dismiss(alert.id)}>
                View alert log
              </Link>
            </div>
            <button
              type="button"
              className="alert-toast-close"
              aria-label="Dismiss alert"
              onClick={() => dismiss(alert.id)}
            >
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <path d="M6 6l12 12M18 6 6 18" />
              </svg>
            </button>
          </div>
        )
      })}
    </div>
  )
}

export default AlertToasts
