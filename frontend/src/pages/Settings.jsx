import { useEffect, useState } from 'react'
import { useAuth } from '../auth/context'
import BackLink from '../components/BackLink'
import { usePreferences } from '../preferences/context'
import { useFormatters } from '../hooks/useFormatters'

const OPTIONS = {
  weightUnit: [
    ['kg', 'Kilograms (kg)'],
    ['lb', 'Pounds (lb)'],
    ['lb_oz', 'Pounds and ounces (lb oz)'],
  ],
}

// Typical newborn readings for previewing the chosen units.
const SAMPLE = { tempC: 36.8, weightKg: 3.6 }

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
    <label className="setting select-row">
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

  // A two-option setting as "left [switch] right"; checked picks the right one.
  const toggle = (key, name, [leftValue, leftText], [rightValue, rightText]) => {
    const right = preferences[key] === rightValue
    return (
      <label className="setting switch-row either-row">
        <span className={right ? undefined : 'on'}>{leftText}</span>
        <input
          type="checkbox"
          role="switch"
          className="switch either"
          aria-label={name}
          checked={right}
          onChange={(e) => setPreference(key, e.target.checked ? rightValue : leftValue)}
        />
        <span className={right ? 'on' : undefined}>{rightText}</span>
      </label>
    )
  }

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
          </section>

          <section className="settings-section">
            <h2>Appearance</h2>
            <label className="setting switch-row either-row">
              <span className={dark ? undefined : 'on'}>Light mode</span>
              <input
                type="checkbox"
                role="switch"
                className="switch either"
                aria-label="Dark mode"
                checked={dark}
                onChange={(e) => setPreference('theme', e.target.checked ? 'dark' : 'light')}
              />
              <span className={dark ? 'on' : undefined}>Dark mode</span>
            </label>
          </section>
        </div>

        <section className="settings-section">
          <h2>Units &amp; time</h2>
          <p className="format-preview" data-testid="format-preview">
            <code>{fmt.temp(SAMPLE.tempC)}</code>
            <code>{fmt.weight(SAMPLE.weightKg)}</code>
            <code>{fmt.time(previewTime)}</code>
          </p>
          {toggle('tempUnit', 'Temperature unit', ['C', 'Celsius (°C)'], ['F', 'Fahrenheit (°F)'])}
          {select('weightUnit', 'Weight', OPTIONS.weightUnit)}
          {toggle('timeFormat', 'Time format', ['12h', '12-hour'], ['24h', '24-hour'])}
        </section>
      </div>
    </section>
  )
}

export default Settings
