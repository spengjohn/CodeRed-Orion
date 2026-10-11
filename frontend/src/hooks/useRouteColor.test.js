import { CAT_COLORS } from '../components/catColors'
import { pickNext } from './useRouteColor'

jest.mock('react-router', () => ({ useLocation: jest.fn() }))

describe('pickNext', () => {
  it('never returns the current color', () => {
    for (const current of CAT_COLORS) {
      for (const r of [0, 0.124, 0.5, 0.999]) {
        const next = pickNext(current, () => r)
        expect(next).not.toBe(current)
        expect(CAT_COLORS).toContain(next)
      }
    }
  })

  it('can reach every other color', () => {
    const seen = new Set()
    for (let i = 0; i < 8; i++) seen.add(pickNext('alert', () => i / 8))
    expect(seen.size).toBe(8)
    expect(seen.has('alert')).toBe(false)
  })
})
