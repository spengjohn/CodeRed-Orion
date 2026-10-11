import { render, screen } from '@testing-library/react'
import CatLoader from './CatLoader'
import CatLogo from './CatLogo'

it('fills the tile with the given color token', () => {
  const { container } = render(<CatLogo color="temp" />)
  expect(container.querySelector('rect')).toHaveAttribute('fill', 'var(--temp)')
  expect(container.querySelector('svg')).toHaveAttribute('aria-hidden', 'true')
})

it('is labelled when given a title', () => {
  render(<CatLogo title="Whisker" />)
  expect(screen.getByRole('img', { name: 'Whisker' })).toBeInTheDocument()
})

it('renders the 3×3 loader as a status', () => {
  const { container } = render(<CatLoader />)
  expect(container.querySelectorAll('svg')).toHaveLength(9)
  expect(screen.getByRole('status')).toHaveTextContent('Loading…')
})