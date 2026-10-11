import { Auth0Provider, useAuth0 } from '@auth0/auth0-react'
import { useCallback, useLayoutEffect, useMemo, useState } from 'react'
import { setAccessTokenGetter } from '../api/client'
import { AuthContext, DEMO_USER, STORAGE_KEY } from './context'

// Auth0 settings from .env. Without a domain and client ID the app runs in
// demo mode: a local signed-in flag and no access token.
const AUTH0 = {
  domain: import.meta.env.VITE_AUTH0_DOMAIN,
  clientId: import.meta.env.VITE_AUTH0_CLIENT_ID,
  // Must equal the backend's AUTH0_AUDIENCE, or it rejects the token.
  audience: import.meta.env.VITE_AUTH0_AUDIENCE,
}

// Exposes useAuth0() through AuthContext, so pages use useAuth() either way,
// and hands its token getter to the API layer for REST and Socket.IO.
function Auth0Bridge({ children }) {
  const auth0 = useAuth0()
  const { getAccessTokenSilently, isAuthenticated, logout } = auth0

  // A layout effect, so the getter is in place before pages' data effects
  // (which run first otherwise, being children) make their first request.
  useLayoutEffect(() => {
    setAccessTokenGetter(isAuthenticated ? () => getAccessTokenSilently() : null)
    return () => setAccessTokenGetter(null)
  }, [isAuthenticated, getAccessTokenSilently])

  const value = useMemo(
    () => ({
      isAuthenticated,
      isLoading: auth0.isLoading,
      user: auth0.user,
      loginWithRedirect: auth0.loginWithRedirect,
      logout: () => logout({ logoutParams: { returnTo: window.location.origin } }),
      getAccessToken: getAccessTokenSilently,
    }),
    [isAuthenticated, auth0.isLoading, auth0.user, auth0.loginWithRedirect, logout, getAccessTokenSilently],
  )

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>
}

// Demo mode: the signed-in flag lives in localStorage so it survives reloads;
// if storage is unavailable, the user starts signed out.
function load() {
  try {
    return localStorage.getItem(STORAGE_KEY) === 'signed-in'
  } catch {
    return false
  }
}

function save(signedIn) {
  try {
    if (signedIn) localStorage.setItem(STORAGE_KEY, 'signed-in')
    else localStorage.removeItem(STORAGE_KEY)
  } catch {
    // Keep working with in-memory state.
  }
}

function DemoAuthProvider({ children }) {
  const [isAuthenticated, setAuthenticated] = useState(load)

  const loginWithRedirect = useCallback(() => {
    save(true)
    setAuthenticated(true)
  }, [])

  const logout = useCallback(() => {
    save(false)
    setAuthenticated(false)
  }, [])

  const value = useMemo(
    () => ({
      isAuthenticated,
      isLoading: false,
      user: isAuthenticated ? DEMO_USER : undefined,
      loginWithRedirect,
      logout,
      getAccessToken: async () => null,
    }),
    [isAuthenticated, loginWithRedirect, logout],
  )

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>
}

function AuthProvider({ children }) {
  if (!AUTH0.domain || !AUTH0.clientId) return <DemoAuthProvider>{children}</DemoAuthProvider>

  return (
    <Auth0Provider
      domain={AUTH0.domain}
      clientId={AUTH0.clientId}
      authorizationParams={{ audience: AUTH0.audience, redirect_uri: window.location.origin }}
      // Keeps the session across reloads; refresh tokens avoid third-party cookies.
      cacheLocation="localstorage"
      useRefreshTokens
    >
      <Auth0Bridge>{children}</Auth0Bridge>
    </Auth0Provider>
  )
}

export default AuthProvider
