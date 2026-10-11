import { fireEvent, render, screen } from '@testing-library/react'
import AuthProvider from '../auth/AuthProvider'
import { STORAGE_KEY } from '../auth/context'
import UserMenu from './UserMenu'

// react-router ships ESM-only dependencies Jest can't load, and the menu only
// needs Link and useNavigate, so stub those.
const mockNavigate = jest.fn()
jest.mock('react-router', () => ({
  Link: ({ to, children, ...props }) => (
    <a href={to} {...props}>
      {children}
    </a>
  ),
  useNavigate: () => mockNavigate,
}))

const renderMenu = () =>
  render(
    <AuthProvider>
      <UserMenu />
    </AuthProvider>,
  )

const openMenu = () => fireEvent.click(screen.getByRole('button', { name: 'Menu' }))

beforeEach(() => {
  mockNavigate.mockClear()
  localStorage.setItem(STORAGE_KEY, 'signed-in')
})
afterEach(() => localStorage.clear())

test('opens to show the user, settings, and log out', () => {
  renderMenu()
  expect(screen.queryByRole('menu')).not.toBeInTheDocument()

  openMenu()
  expect(screen.getByRole('menu')).toHaveTextContent('Demo Parent')
  expect(screen.getByRole('menuitem', { name: 'Settings' })).toHaveAttribute('href', '/settings')
  expect(screen.getByRole('menuitem', { name: 'Log out' })).toBeInTheDocument()
})

test('choosing user settings closes the menu', () => {
  renderMenu()
  openMenu()
  fireEvent.click(screen.getByRole('menuitem', { name: 'Settings' }))
  expect(screen.queryByRole('menu')).not.toBeInTheDocument()
})

test('closes on Escape and on an outside click', () => {
  renderMenu()
  openMenu()
  fireEvent.keyDown(document, { key: 'Escape' })
  expect(screen.queryByRole('menu')).not.toBeInTheDocument()

  openMenu()
  fireEvent.mouseDown(document.body)
  expect(screen.queryByRole('menu')).not.toBeInTheDocument()
})

test('log out signs out and returns to the splash', () => {
  renderMenu()
  openMenu()
  fireEvent.click(screen.getByRole('menuitem', { name: 'Log out' }))

  expect(localStorage.getItem(STORAGE_KEY)).toBeNull()
  expect(mockNavigate).toHaveBeenCalledWith('/', { replace: true })
})
