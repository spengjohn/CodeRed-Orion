import { useState } from 'react'
import { alertKind, alertTitle } from '../api/alerts'
import AlertIcon from '../components/AlertIcon'
import BackLink from '../components/BackLink'
import CatLoader from '../components/CatLoader'
import CatLogo from '../components/CatLogo'
import { useAlertSnapshot } from '../hooks/useAlertSnapshot'
import { useAlerts } from '../hooks/useAlerts'
import { useFormatters } from '../hooks/useFormatters'

// A snapshot thumbnail; fetched only when the user opens it, to keep the page light.
function Snapshot({ alert }) {
  const [open, setOpen] = useState(Boolean(alert.image))
  const { src, failed } = useAlertSnapshot(alert, open)

  if (!alert.hasSnapshot) return null
  if (!open) {
    return (
      <button type="button" className="wbtn gho alert-snap-btn" onClick={() => setOpen(true)}>
        Show snapshot
      </button>
    )
  }
  if (failed) return <span className="muted">Snapshot unavailable</span>
  return src ? (
    <img className="alert-snap" src={src} alt="Crib camera when the alert fired" />
  ) : (
    <span className="skeleton alert-snap" aria-label="Loading snapshot" />
  )
}

// The alert log: every emergency alert from the crib, newest first, with new
// ones appearing at the top as they arrive.
function Alerts() {
  const fmt = useFormatters()
  const { alerts, loading, error } = useAlerts()

  return (
    <section className="page alerts-page">
      <div className="page-head">
        <div className="title-row">
          <BackLink />
          <h1>Alerts</h1>
        </div>
        <p>
          Every alert the crib has raised. Each one also called the registered phone numbers.
        </p>
      </div>

      {error && alerts.length === 0 && <p className="error">Couldn't load alerts: {error.message}</p>}

      {loading && alerts.length === 0 ? (
        <CatLoader label="Loading alerts…" />
      ) : alerts.length === 0 && !error ? (
        <div className="panel alerts-empty">
          <CatLogo size={48} color="ok" />
          <strong>No alerts</strong>
          <span className="muted">All quiet. Alerts from the crib will show up here.</span>
        </div>
      ) : (
        <ol className="alert-log">
          {alerts.map((alert) => {
            const kind = alertKind(alert.code)
            return (
              <li key={alert.id} className="alert-row" style={{ '--m': `var(--${kind})` }}>
                <AlertIcon kind={kind} />
                <div className="alert-row-body">
                  <strong>{alertTitle(alert.description)}</strong>
                  <span className="muted">
                    {fmt.dateTime(alert.ts)} · {fmt.relative(alert.ts)}
                    {alert.code != null && ` · code ${alert.code}`}
                  </span>
                  {alert.details.message && <span>{alert.details.message}</span>}
                  <Snapshot alert={alert} />
                </div>
              </li>
            )
          })}
        </ol>
      )}
    </section>
  )
}

export default Alerts
