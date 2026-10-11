import { createContext, useContext } from 'react'

export const STORAGE_KEY = 'whisker.auth'

// Shown in demo mode (no Auth0 configured); Auth0 supplies the real profile.
export const DEMO_USER = { name: 'Demo Parent', email: 'parent@example.com' }

export const AuthContext = createContext(null)

// Same in Auth0 and demo mode, modeled on useAuth0():
// { isAuthenticated, isLoading, user, loginWithRedirect, logout, getAccessToken }
export function useAuth() {
  const value = useContext(AuthContext)
  if (!value) throw new Error('useAuth must be used inside <AuthProvider>')
  return value
}
