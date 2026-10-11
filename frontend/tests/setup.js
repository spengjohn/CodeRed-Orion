import '@testing-library/jest-dom'
import { TextDecoder, TextEncoder } from 'node:util'

// jsdom lacks TextEncoder/TextDecoder; the Auth0 SDK reads them on import.
globalThis.TextEncoder ??= TextEncoder
globalThis.TextDecoder ??= TextDecoder

// jsdom has no ResizeObserver; Recharts' ResponsiveContainer needs one.
globalThis.ResizeObserver ??= class {
  observe() {}
  unobserve() {}
  disconnect() {}
}
