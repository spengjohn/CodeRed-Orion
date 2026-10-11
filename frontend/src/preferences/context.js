import { createContext, useContext } from 'react'

export const STORAGE_KEY = 'whisker.preferences'

export const DEFAULT_PREFERENCES = {
  tempUnit: 'C', // 'C' | 'F'
  weightUnit: 'kg', // 'kg' | 'lb' | 'lb_oz'
  timeFormat: '12h', // '12h' | '24h'
  theme: 'system', // 'system' | 'light' | 'dark'
}

export const PreferencesContext = createContext(null)

// { preferences, setPreference(key, value) }
export function usePreferences() {
  const value = useContext(PreferencesContext)
  if (!value) throw new Error('usePreferences must be used inside <PreferencesProvider>')
  return value
}
