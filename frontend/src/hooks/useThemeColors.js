import { useEffect, useState } from 'react'

// Token names from src/tokens.css that charts use.
const TOKENS = ['text', 'muted', 'border', 'surface', 'secondary', 'temp', 'weight', 'breath', 'activity']

function read() {
  const style = getComputedStyle(document.documentElement)
  return Object.fromEntries(TOKENS.map((name) => [name, style.getPropertyValue(`--${name}`).trim()]))
}

// Current token colors as plain values. Recharts sets SVG attributes, which
// can't use var(), so charts read the colors here; they update when the
// system switches between light and dark, or the theme setting forces one.
export function useThemeColors() {
  const [colors, setColors] = useState(read)

  useEffect(() => {
    const query = window.matchMedia?.('(prefers-color-scheme: dark)')
    const update = () => setColors(read())
    query?.addEventListener('change', update)
    const observer = new MutationObserver(update)
    observer.observe(document.documentElement, { attributeFilter: ['data-theme'] })
    return () => {
      query?.removeEventListener('change', update)
      observer.disconnect()
    }
  }, [])

  return colors
}
