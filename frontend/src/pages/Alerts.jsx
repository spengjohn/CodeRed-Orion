import { useState } from 'react'
import { alertKind, alertTitle } from '../api/alerts'
import AlertIcon from '../components/AlertIcon'
import BackLink from '../components/BackLink'
import CatLoader from '../components/CatLoader'
import CatLogo from '../components/CatLogo'
import ConfirmDialog from '../components/ConfirmDialog'
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
    <div className="alert-snap alert-snap-loading">
      <CatLoader inline size={24} label="Loading snapshot…" />
    </div>
  )
}

// The alert log: every emergency alert from the crib, newest first, with new
// ones appearing at the top as they arrive.
function Alerts() {
  const fmt = useFormatters()
  const { alerts, loading, error, clear } = useAlerts()
  const [confirmOpen, setConfirmOpen] = useState(false)
  const [clearing, setClearing] = useState(false)
  const [clearError, setClearError] = useState(null)

  function openConfirm() {
    setClearError(null)
    setConfirmOpen(true)
  }

  // The dialog stays open on failure so the error shows where the user is looking.
  async function onClear() {
    setClearing(true)
    setClearError(null)
    try {
      await clear()
      setConfirmOpen(false)
    } catch (err) {
      setClearError(err)
    } finally {
      setClearing(false)
    }
  }

  const n = alerts.length

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

      {confirmOpen && (
        <ConfirmDialog
          title={n === 1 ? 'Clear this alert?' : `Clear all ${n} alerts?`}
          message={`This permanently deletes ${n === 1 ? 'the alert and its snapshot' : 'these alerts and their snapshots'}. This can't be undone.`}
          confirmLabel="Clear alerts"
          busyLabel="Clearing…"
          busy={clearing}
          error={clearError}
          onConfirm={onClear}
          onCancel={() => setConfirmOpen(false)}
        />
      )}
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
        <>
          {alerts.length > 0 && (
            <div className="alerts-toolbar">
              <button type="button" className="wbtn alerts-clear" onClick={openConfirm}>
                Clear alerts
              </button>
            </div>
          )}
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
        </>
      )}
    </section>
  )
}

export default Alerts
