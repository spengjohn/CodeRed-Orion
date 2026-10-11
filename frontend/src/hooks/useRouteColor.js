import { useState } from 'react'
import { useLocation } from 'react-router'
import { CAT_COLORS } from '../components/catColors'

// A random logo color other than the current one.
export function pickNext(current, rand = Math.random) {
  const others = CAT_COLORS.filter((c) => c !== current)
  return others[Math.floor(rand() * others.length)]
}

// The header logo's color: starts on primary, then changes on every navigation.
export function useRouteColor() {
  const { pathname } = useLocation()
  const [state, setState] = useState({ pathname, color: 'primary' })

  // Adjust state during render when the path changes, rather than in an effect.
  if (state.pathname !== pathname) {
    const next = { pathname, color: pickNext(state.color) }
    setState(next)
    return next.color
  }
  return state.color
}
