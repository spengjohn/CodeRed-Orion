import { lazy, Suspense } from 'react'
import { BrowserRouter, Link, Route, Routes } from 'react-router'
import { useAuth } from './auth/context'
import RequireAuth from './auth/RequireAuth'
import AlertToasts from './components/AlertToasts'
import CatLoader from './components/CatLoader'
import CatLogo from './components/CatLogo'
import UserMenu from './components/UserMenu'
import { useRouteColor } from './hooks/useRouteColor'
import Alerts from './pages/Alerts'
import Landing from './pages/Landing'
import Dashboard from './pages/Dashboard'
import Settings from './pages/Settings'
import './App.css'

// Analytics pages pull in the charting library, so load them on demand.
const TemperatureAnalytics = lazy(() => import('./pages/TemperatureAnalytics'))
const WeightAnalytics = lazy(() => import('./pages/WeightAnalytics'))
const BreathingAnalytics = lazy(() => import('./pages/BreathingAnalytics'))
const ActivityAnalytics = lazy(() => import('./pages/ActivityAnalytics'))

// The logo takes a new color on each navigation, so this lives inside the router.
function Header({ isAuthenticated }) {
  const color = useRouteColor()

  return (
    <nav className="nav">
      <Link className="brand" to={isAuthenticated ? '/dashboard' : '/'}>
        {/* Keyed on color so it remounts and replays its pop. */}
        <CatLogo key={color} color={color} size={28} className="cat-pop" />
        <span>Whisker</span>
      </Link>
      {isAuthenticated && <UserMenu />}
    </nav>
  )
}

function App() {
  const { isAuthenticated } = useAuth()

  return (
    <BrowserRouter>
      <Header isAuthenticated={isAuthenticated} />
      <main>
        <Suspense fallback={<CatLoader />}>
          <Routes>
            <Route path="/" element={<Landing />} />
            <Route element={<RequireAuth />}>
              <Route path="/dashboard" element={<Dashboard />} />
              <Route path="/analytics/temperature" element={<TemperatureAnalytics />} />
              <Route path="/analytics/weight" element={<WeightAnalytics />} />
              <Route path="/analytics/breathing" element={<BreathingAnalytics />} />
              <Route path="/analytics/activity" element={<ActivityAnalytics />} />
              <Route path="/alerts" element={<Alerts />} />
              <Route path="/settings" element={<Settings />} />
            </Route>
          </Routes>
        </Suspense>
      </main>
      {/* Emergency alert pop-ups, on every page while signed in. */}
      {isAuthenticated && <AlertToasts />}
    </BrowserRouter>
  )
}

export default App
