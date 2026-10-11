import { render, screen } from '@testing-library/react'
import PreferencesProvider from '../preferences/PreferencesProvider'
import { STORAGE_KEY } from '../preferences/context'
import { formatTime } from '../utils/time'
import { useFormatters } from './useFormatters'

const MS = Date.UTC(2025, 0, 15, 12, 0)

function Time() {
  const fmt = useFormatters()
  return <span>{fmt.time(MS)}</span>
}

beforeEach(() => localStorage.clear())

test('formats times in the device time zone', () => {
  render(
    <PreferencesProvider>
      <Time />
    </PreferencesProvider>,
  )
  expect(screen.getByText(formatTime(MS, { timeFormat: '12h' }))).toBeInTheDocument()
})

test('ignores a time zone saved by older versions', () => {
  // Pick a saved zone that differs from the device's, so the test would catch it being used.
  const device = Intl.DateTimeFormat().resolvedOptions().timeZone
  const saved = device === 'Asia/Tokyo' ? 'America/Chicago' : 'Asia/Tokyo'
  localStorage.setItem(STORAGE_KEY, JSON.stringify({ timeZone: saved }))

  render(
    <PreferencesProvider>
      <Time />
    </PreferencesProvider>,
  )
  expect(screen.getByText(formatTime(MS, { timeFormat: '12h' }))).toBeInTheDocument()
})
