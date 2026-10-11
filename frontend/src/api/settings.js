import { get, put } from './client'

// Unused: the backend has no settings endpoint yet, and user preferences live
// in localStorage (src/preferences). Paths are placeholders.

export const getSettings = () => get('/settings')

export const updateSettings = (settings) => put('/settings', settings)
