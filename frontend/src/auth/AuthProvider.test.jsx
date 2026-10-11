import { fireEvent, render, screen } from '@testing-library/react'
import AuthProvider from './AuthProvider'
import { STORAGE_KEY, useAuth } from './context'

function Status() {
  const { isAuthenticated, user, loginWithRedirect, logout } = useAuth()
  return isAuthenticated ? (
    <button type="button" onClick={() => logout()}>
      {user.name}
    </button>
  ) : (
    <button type="button" onClick={() => loginWithRedirect()}>
      signed out
    </button>
  )
}

const renderWithProvider = () =>
  render(
    <AuthProvider>
      <Status />
    </AuthProvider>,
  )

beforeEach(() => localStorage.clear())

test('starts signed out', () => {
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('signed out')
})

test('signing in survives a reload, and logging out clears it', () => {
  const { unmount } = renderWithProvider()
  fireEvent.click(screen.getByRole('button'))
  expect(screen.getByRole('button')).toHaveTextContent('Demo Parent')

  unmount()
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('Demo Parent')

  fireEvent.click(screen.getByRole('button'))
  expect(screen.getByRole('button')).toHaveTextContent('signed out')
  expect(localStorage.getItem(STORAGE_KEY)).toBeNull()
})

test('treats unexpected stored values as signed out', () => {
  localStorage.setItem(STORAGE_KEY, '{not json')
  renderWithProvider()
  expect(screen.getByRole('button')).toHaveTextContent('signed out')
})
