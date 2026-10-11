import { Navigate } from 'react-router'
import { useAuth } from '../auth/context'
import CatLogo from '../components/CatLogo'

// Splash for signed-out visitors. Signing in (Auth0's hosted login once it's
// configured) returns here, and signed-in users go straight to the dashboard.
function Landing() {
  const { isAuthenticated, isLoading, loginWithRedirect } = useAuth()
  // Returning from Auth0's login: wait instead of flashing the splash.
  if (isLoading) return null
  if (isAuthenticated) return <Navigate to="/dashboard" replace />

  return (
    <section className="page landing">
      <CatLogo size={120} className="splash-logo cat-pop" />
      <h1>Whisker</h1>
      <p className="lede">
        Smart crib monitoring. Sensors in the crib keep watch while your baby sleeps, and you can
        check in from any browser: live readings now, and charts of the last hour to the last 30
        days.
      </p>
      <button type="button" className="wbtn pri" onClick={() => loginWithRedirect()}>
        Sign in
      </button>
    </section>
  )
}

export default Landing
