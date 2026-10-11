import request from 'supertest'

// Pick up BACKEND_URL (or BACKEND_HOST/BACKEND_PORT) and DEV_AUTH_TOKEN from .env.
try {
  process.loadEnvFile('.env')
} catch {
  // No .env: fall back to the defaults below.
}

// Contract tests run against a running backend: local Flask from .env by
// default, or any deployment via API_URL or BACKEND_URL (e.g. https://whisker.health).
export const API_URL =
  process.env.API_URL ??
  process.env.BACKEND_URL ??
  `http://${process.env.BACKEND_HOST || 'localhost'}:${process.env.BACKEND_PORT || 5000}`

export const api = () => request(API_URL)

// An Auth0 access token for the protected routes and Socket.IO (the backend
// verifies it against AUTH0_AUDIENCE). Copy one from a signed-in browser
// session or Auth0's API test tab. Defaults to DEV_AUTH_TOKEN, which the dev
// server's proxy also uses.
export const AUTH_TOKEN = process.env.AUTH_TOKEN ?? process.env.DEV_AUTH_TOKEN

// Supertest request headers: a Bearer token when AUTH_TOKEN is set.
export const authHeaders = () => (AUTH_TOKEN ? { Authorization: `Bearer ${AUTH_TOKEN}` } : {})

// Fail early with a clear message instead of a wall of connection errors.
export async function requireBackend() {
  try {
    await api().get('/api/health').timeout(3000)
  } catch {
    throw new Error(
      `Backend not reachable at ${API_URL}. Start it from backend/ with ` +
        '"flask --app app run", or set BACKEND_URL or API_URL to a running backend.',
    )
  }
}
