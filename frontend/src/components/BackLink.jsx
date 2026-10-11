import { Link } from 'react-router'

// Back arrow beside a page heading (analytics, settings). Links to the dashboard rather
// than using history, so it works when the page was opened directly.
function BackLink() {
  return (
    <Link className="icon-btn" to="/dashboard" aria-label="Back to dashboard">
      <svg viewBox="0 0 24 24" width="22" height="22" aria-hidden="true">
        <path
          d="M15 5l-7 7 7 7"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
      </svg>
    </Link>
  )
}

export default BackLink
