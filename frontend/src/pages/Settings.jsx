import { useEffect, useState } from 'react'
import { useAuth } from '../auth/context'
import BackLink from '../components/BackLink'
import { usePreferences } from '../preferences/context'
import { useFormatters } from '../hooks/useFormatters'

const OPTIONS = {
  tempUnit: [
    ['C', 'Celsius (°C)'],
    ['F', 'Fahrenheit (°F)'],
  ],
  weightUnit: [
    ['kg', 'Kilograms (kg)'],
    ['lb', 'Pounds (lb)'],
    ['lb_oz', 'Pounds and ounces (lb oz)'],
  ],
  timeFormat: [
    ['12h', '12-hour (2:30 PM)'],
    ['24h', '24-hour (14:30)'],
  ],
}

const DARK_QUERY = '(prefers-color-scheme: dark)'

// Whether the system is in dark mode, kept current. False where matchMedia is missing.
function useSystemDark() {
  const [dark, setDark] = useState(() => window.matchMedia?.(DARK_QUERY).matches ?? false)

  useEffect(() => {
    const query = window.matchMedia?.(DARK_QUERY)
    const update = () => setDark(query.matches)
    query?.addEventListener('change', update)
    return () => query?.removeEventListener('change', update)
  }, [])

  return dark
}

function Settings() {
  const { user } = useAuth()
  const { preferences, setPreference } = usePreferences()
  const fmt = useFormatters()
  const systemDark = useSystemDark()
  // Fixed sample time for previewing the chosen format.
  const [previewTime] = useState(() => Date.now())

  // Follows the system until the switch is flipped, which saves light or dark.
  const dark = preferences.theme === 'dark' || (preferences.theme === 'system' && systemDark)

  const select = (key, label, options) => (
    <label className="setting">
      <span>{label}</span>
      <select value={preferences[key]} onChange={(e) => setPreference(key, e.target.value)}>
        {options.map(([value, text]) => (
          <option key={value} value={value}>
            {text}
          </option>
        ))}
      </select>
    </label>
  )

  return (
    <section className="page">
      <div className="page-head">
        <div className="title-row">
          <BackLink />
          <h1>Settings</h1>
        </div>
      </div>

      {/* One card; its sections sit side by side when there's room, stacked otherwise. */}
      <div className="panel settings-card">
        {/* Appearance always stays under Account, even when the columns spread out. */}
        <div className="settings-stack">
          <section className="settings-section">
            <h2>Account</h2>
            <div className="setting">
              <span>Name</span>
              <strong>{user?.name}</strong>
            </div>
            <div className="setting">
              <span>Email</span>
              <strong>{user?.email}</strong>
            </div>
            <p className="muted">Your sign-in is managed by Auth0 (demo account for now).</p>
          </section>

          <section className="settings-section">
            <h2>Appearance</h2>
            <label className="setting switch-row">
              <span>Dark mode</span>
              <input
                type="checkbox"
                role="switch"
                className="switch"
                checked={dark}
                onChange={(e) => setPreference('theme', e.target.checked ? 'dark' : 'light')}
              />
            </label>
          </section>
        </div>

        <section className="settings-section">
          <h2>Units &amp; time</h2>
          {select('tempUnit', 'Temperature', OPTIONS.tempUnit)}
          {select('weightUnit', 'Weight', OPTIONS.weightUnit)}
          {select('timeFormat', 'Time format', OPTIONS.timeFormat)}
          <p className="muted">Preview: {fmt.dateTime(previewTime)}</p>
          <p className="muted">Preferences are saved in this browser.</p>
        </section>
      </div>
    </section>
  )
}

export default Settings
