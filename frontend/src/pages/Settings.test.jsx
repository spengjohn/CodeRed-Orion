import { fireEvent, render, screen } from '@testing-library/react'
import { AuthContext, DEMO_USER } from '../auth/context'
import PreferencesProvider from '../preferences/PreferencesProvider'
import { STORAGE_KEY } from '../preferences/context'
import Settings from './Settings'

// react-router ships ESM-only dependencies Jest can't load; BackLink only needs Link.
jest.mock('react-router', () => ({
  Link: ({ to, children, ...props }) => (
    <a href={to} {...props}>
      {children}
    </a>
  ),
}))

const renderSettings = () =>
  render(
    <AuthContext.Provider value={{ user: DEMO_USER }}>
      <PreferencesProvider>
        <Settings />
      </PreferencesProvider>
    </AuthContext.Provider>,
  )

beforeEach(() => {
  localStorage.clear()
  delete document.documentElement.dataset.theme
})

test('shows account info and preferences in one card', () => {
  renderSettings()
  expect(screen.getByText(DEMO_USER.email)).toBeInTheDocument()
  expect(screen.getByRole('heading', { name: 'Account' })).toBeInTheDocument()
  expect(screen.getByRole('heading', { name: 'Appearance' })).toBeInTheDocument()
})

test('dark mode switch follows the system, then saves an explicit choice', () => {
  renderSettings()
  const toggle = screen.getByRole('switch', { name: 'Dark mode' })
  // No matchMedia in jsdom, so the system reads as light.
  expect(toggle).not.toBeChecked()
  expect(document.documentElement).not.toHaveAttribute('data-theme')

  fireEvent.click(toggle)
  expect(toggle).toBeChecked()
  expect(document.documentElement).toHaveAttribute('data-theme', 'dark')
  expect(JSON.parse(localStorage.getItem(STORAGE_KEY))).toMatchObject({ theme: 'dark' })

  fireEvent.click(toggle)
  expect(toggle).not.toBeChecked()
  expect(document.documentElement).toHaveAttribute('data-theme', 'light')
})
