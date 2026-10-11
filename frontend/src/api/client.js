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

// What the user sees for a failed request. Pages render error.message, so it
// must never include the API path or browser internals.
function errorMessage(status) {
  if (status === 401) return 'Your session has expired. Sign in again.'
  if (status === 403) return "You don't have access to this."
  if (status === 404) return 'Not found.'
  if (status >= 500) return 'The server ran into a problem. Try again shortly.'
  return 'Request failed.'
}

async function request(path, options = {}) {
  const method = options.method ?? 'GET'
  const token = await getAccessToken()
  let res
  try {
    res = await fetch(`${BASE_URL}${path}`, {
      ...options,
      headers: {
        'Content-Type': 'application/json',
        ...(token && { Authorization: `Bearer ${token}` }),
        ...options.headers,
      },
    })
  } catch (cause) {
    console.warn(`${method} ${path} failed:`, cause)
    throw Object.assign(new Error("Can't reach the Whisker server."), { path, cause })
  }
  if (!res.ok) {
    // Details stay in the console and on the error object, out of the UI.
    console.warn(`${method} ${path} failed: ${res.status}`)
    throw Object.assign(new Error(errorMessage(res.status)), { status: res.status, path })
  }
  return res.status === 204 ? null : res.json()
}

export const get = (path) => request(path)

export const put = (path, body) =>
  request(path, { method: 'PUT', body: JSON.stringify(body) })

export const del = (path) => request(path, { method: 'DELETE' })
