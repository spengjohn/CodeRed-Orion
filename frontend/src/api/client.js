// Base URL for the Whisker backend. Set VITE_API_BASE_URL in a .env file;
// falls back to /api so nginx (production) or the Vite proxy (dev) can route it.
export const BASE_URL = import.meta.env.VITE_API_BASE_URL ?? '/api'

// Where requests get their Auth0 access token. The auth provider registers
// Auth0's getAccessTokenSilently here once signed in; demo mode has no token.
let tokenGetter = async () => null

export function setAccessTokenGetter(getter) {
  tokenGetter = getter ?? (async () => null)
}

// The current access token, or null when signed out, in demo mode, or when
// Auth0 can't refresh it (the backend then answers 401).
export async function getAccessToken() {
  try {
    return await tokenGetter()
  } catch {
    return null
  }
}

async function request(path, options = {}) {
  const token = await getAccessToken()
  const res = await fetch(`${BASE_URL}${path}`, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...(token && { Authorization: `Bearer ${token}` }),
      ...options.headers,
    },
  })
  if (!res.ok) {
    const message =
      res.status === 401
        ? 'Your session has expired. Sign in again.'
        : `${options.method ?? 'GET'} ${path} failed: ${res.status}`
    throw Object.assign(new Error(message), { status: res.status })
  }
  return res.status === 204 ? null : res.json()
}

export const get = (path) => request(path)

export const put = (path, body) =>
  request(path, { method: 'PUT', body: JSON.stringify(body) })
