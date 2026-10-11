import { createBrowserRouter, Link, Outlet } from 'react-router'
import { RouterProvider } from 'react-router/dom'
import { useAuth } from './auth/context'
import RequireAuth from './auth/RequireAuth'
import AlertToasts from './components/AlertToasts'
import CatLoader from './components/CatLoader'
import CatLogo from './components/CatLogo'
import UserMenu from './components/UserMenu'
import { useRouteColor } from './hooks/useRouteColor'
import Alerts from './pages/Alerts'
import * as analytics from './pages/analyticsRoutes'
import Landing from './pages/Landing'
import Dashboard from './pages/Dashboard'
import Settings from './pages/Settings'
import './App.css'

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

// Chrome around every page. Header and toasts use router hooks, so they render
// inside the router as the root route's element.
function AppLayout() {
  const { isAuthenticated } = useAuth()

  return (
    <>
      <Header isAuthenticated={isAuthenticated} />
      <main>
        <Outlet />
      </main>
      {/* Emergency alert pop-ups, on every page while signed in. */}
      {isAuthenticated && <AlertToasts />}
    </>
  )
}

// A data router (not <BrowserRouter>) because only RouterProvider runs
// <Link viewTransition> navigations through document.startViewTransition.
const router = createBrowserRouter([
  {
    element: <AppLayout />,
    // Shown while a lazy page loads on a direct visit.
    hydrateFallbackElement: <CatLoader />,
    children: [
      { path: '/', element: <Landing /> },
      {
        element: <RequireAuth />,
        children: [
          { path: '/dashboard', element: <Dashboard /> },
          { path: '/analytics/temperature', lazy: analytics.temperature },
          { path: '/analytics/weight', lazy: analytics.weight },
          { path: '/analytics/breathing', lazy: analytics.breathing },
          { path: '/analytics/activity', lazy: analytics.activity },
          { path: '/alerts', element: <Alerts /> },
          { path: '/settings', element: <Settings /> },
        ],
      },
    ],
  },
])

function App() {
  return <RouterProvider router={router} />
}

export default App
