import { fireEvent, render, screen } from '@testing-library/react'
import PreferencesProvider from './PreferencesProvider'
import { STORAGE_KEY, usePreferences } from './context'

function TempUnit() {
  const { preferences, setPreference } = usePreferences()
  return (
    <button type="button" onClick={() => setPreference('tempUnit', 'F')}>
      {preferences.tempUnit}
    </button>
  )
}

const renderWithProvider = () =>
  render(
    <PreferencesProvider>
      <TempUnit />
    </PreferencesProvider>,
  )

beforeEach(() => localStorage.clear())

test('starts with defaults', () => {
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('C')
})

test('saves changes to localStorage and restores them on the next load', () => {
  const { unmount } = renderWithProvider()
  fireEvent.click(screen.getByRole('button'))

  expect(screen.getByRole('button')).toHaveTextContent('F')
  expect(JSON.parse(localStorage.getItem(STORAGE_KEY))).toMatchObject({ tempUnit: 'F' })

  unmount()
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('F')
})

test('falls back to defaults when stored data is corrupt', () => {
  localStorage.setItem(STORAGE_KEY, '{not json')
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('C')
})

function ThemeButtons() {
  const { setPreference } = usePreferences()
  return ['system', 'dark'].map((theme) => (
    <button key={theme} type="button" onClick={() => setPreference('theme', theme)}>
      {theme}
    </button>
  ))
}

test('forces the chosen theme on <html>, and clears it for system', () => {
  delete document.documentElement.dataset.theme
  render(
    <PreferencesProvider>
      <ThemeButtons />
    </PreferencesProvider>,
  )
  expect(document.documentElement).not.toHaveAttribute('data-theme')

  fireEvent.click(screen.getByRole('button', { name: 'dark' }))
  expect(document.documentElement).toHaveAttribute('data-theme', 'dark')

  fireEvent.click(screen.getByRole('button', { name: 'system' }))
  expect(document.documentElement).not.toHaveAttribute('data-theme')
})
