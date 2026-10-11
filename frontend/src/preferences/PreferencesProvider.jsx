import { useCallback, useEffect, useMemo, useState } from 'react'
import { DEFAULT_PREFERENCES, PreferencesContext, STORAGE_KEY } from './context'

// Preferences live in localStorage so they survive reloads. Storage can be
// unavailable (private mode, blocked site data), so failures fall back to defaults.
function load() {
  try {
    return { ...DEFAULT_PREFERENCES, ...JSON.parse(localStorage.getItem(STORAGE_KEY)) }
  } catch {
    return DEFAULT_PREFERENCES
  }
}

function save(preferences) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(preferences))
  } catch {
    // Keep working with in-memory preferences.
  }
}

function PreferencesProvider({ children }) {
  const [preferences, setPreferences] = useState(load)

  const setPreference = useCallback((key, value) => {
    setPreferences((prev) => {
      const next = { ...prev, [key]: value }
      save(next)
      return next
    })
  }, [])

  // tokens.css forces a theme via <html data-theme>, and follows the system without it.
  // index.html applies the stored choice before first paint; this keeps it in sync.
  useEffect(() => {
    const root = document.documentElement
    if (preferences.theme === 'light' || preferences.theme === 'dark') {
      root.dataset.theme = preferences.theme
    } else {
      delete root.dataset.theme
    }
  }, [preferences.theme])

  const value = useMemo(() => ({ preferences, setPreference }), [preferences, setPreference])

  return <PreferencesContext.Provider value={value}>{children}</PreferencesContext.Provider>
}

export default PreferencesProvider
