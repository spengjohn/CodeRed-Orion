import { Navigate, Outlet } from 'react-router'
import { useAuth } from './context'

// Wraps every route except the splash. Signed-out visitors go back to the
// splash; with Auth0 this can call loginWithRedirect() instead.
function RequireAuth() {
  const { isAuthenticated, isLoading } = useAuth()
  if (isLoading) return <p className="page muted">Loading…</p>
  return isAuthenticated ? <Outlet /> : <Navigate to="/" replace />
}

export default RequireAuth
