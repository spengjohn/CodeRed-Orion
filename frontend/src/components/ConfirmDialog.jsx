import { useEffect, useId, useRef } from 'react'
import { createPortal } from 'react-dom'
import AlertIcon from './AlertIcon'

// An in-app "are you sure?" pop-up for destructive actions, styled like the
// timeline's alert pop-up. Cancel gets focus first; Escape or a click on the
// backdrop cancels, except while the action is running.
function ConfirmDialog({ title, message, confirmLabel, busyLabel, busy, error, onConfirm, onCancel }) {
  const titleId = useId()
  const cancelButton = useRef(null)

  useEffect(() => {
    cancelButton.current?.focus()
  }, [])

  useEffect(() => {
    if (busy) return undefined
    const onKey = (e) => {
      if (e.key === 'Escape') onCancel()
    }
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [busy, onCancel])

  // Portaled to <body>: .page's page-in transform would otherwise contain the
  // fixed overlay, dimming only the page column (same as CatLoader).
  return createPortal(
    <div className="tl-popover-overlay" onClick={busy ? undefined : onCancel} role="presentation">
      <div
        className="tl-popover confirm-dialog"
        role="alertdialog"
        aria-modal="true"
        aria-labelledby={titleId}
        style={{ '--m': 'var(--alert)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div className="tl-popover-head">
          <AlertIcon kind="alert" />
          <div className="tl-popover-meta">
            <strong id={titleId}>{title}</strong>
          </div>
        </div>
        <p className="tl-popover-msg">{message}</p>
        {error && <p className="error">{error.message}</p>}
        <div className="tl-popover-actions">
          <button type="button" className="wbtn gho" ref={cancelButton} onClick={onCancel} disabled={busy}>
            Cancel
          </button>
          <button type="button" className="wbtn pri confirm-danger" onClick={onConfirm} disabled={busy}>
            {busy ? (busyLabel ?? confirmLabel) : confirmLabel}
          </button>
        </div>
      </div>
    </div>,
    document.body,
  )
}

export default ConfirmDialog
