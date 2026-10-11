// Analytics pages pull in the charting library, so they load on demand as lazy
// routes. The router waits for a lazy route before committing the navigation,
// so a metric card's view transition always morphs into the real page.
const page = (load) => () => load().then((module) => ({ Component: module.default }))

export const temperature = page(() => import('./TemperatureAnalytics'))
export const weight = page(() => import('./WeightAnalytics'))
export const breathing = page(() => import('./BreathingAnalytics'))
export const activity = page(() => import('./ActivityAnalytics'))

// Fetch the chunks ahead of a click so the first card tap doesn't wait on the network.
export function preloadAnalytics() {
  for (const load of [temperature, weight, breathing, activity]) {
    load().catch(() => {}) // a failed fetch retries on navigation instead
  }
}
