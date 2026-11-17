/**
 * Host half of the session-manager bundle.
 *
 * The browser half is a panel: it lists sessions and asks this half to delete
 * one. Deletion must happen here, because only the Host can touch the durable
 * session store, and the Web client reaches it through two same-origin JSON
 * routes registered on `ctx.webServer`. They are `WebRoute` handlers rather
 * than resource routes, because a resource only answers GET/HEAD.
 */
import { promises as fsp } from 'node:fs'
import { homedir } from 'node:os'
import path from 'node:path'

export const inject = ['webServer']

const LIST_PATH = '/plugin-session-manager/list'
const DELETE_PATH = '/plugin-session-manager/delete'
/** Custom header the panel sends; a cross-site form cannot set it without a preflight this route never grants. */
const CSRF_HEADER = 'x-dsh-session-manager'
const SESSION_ID = /^session-[A-Za-z0-9_-]{1,96}$/
const MAX_BODY = 64 * 1024

function dshHome() {
  const configured = process.env.DSH_HOME
  if (typeof configured === 'string' && configured.trim() !== '') return configured
  return path.join(homedir(), '.dsh')
}

async function directorySize(directory) {
  let total = 0
  const entries = await fsp.readdir(directory, { withFileTypes: true }).catch(() => [])
  for (const entry of entries) {
    const full = path.join(directory, entry.name)
    if (entry.isDirectory()) {
      total += await directorySize(full)
      continue
    }
    const stat = await fsp.stat(full).catch(() => null)
    if (stat !== null) total += stat.size
  }
  return total
}

export function apply(ctx) {
  const home = dshHome()
  const sessionsRoot = path.join(home, 'sessions')
  const projectionCache = path.join(home, 'storages', 'session_projcache', 'sessions')

  function send(res, status, payload) {
    const body = JSON.stringify(payload)
    res.writeHead(status, {
      'content-type': 'application/json; charset=utf-8',
      'content-length': Buffer.byteLength(body),
      'cache-control': 'no-store',
    })
    res.end(body)
  }

  function readJsonBody(req) {
    return new Promise(resolve => {
      const chunks = []
      let size = 0
      let settled = false
      const finish = value => {
        if (settled) return
        settled = true
        resolve(value)
      }
      req.on('data', chunk => {
        size += chunk.length
        if (size > MAX_BODY) {
          finish(null)
          req.destroy()
          return
        }
        chunks.push(chunk)
      })
      req.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf8')
        if (text === '') return finish({})
        try {
          const parsed = JSON.parse(text)
          finish(parsed !== null && typeof parsed === 'object' ? parsed : null)
        } catch {
          finish(null)
        }
      })
      req.on('error', () => finish(null))
    })
  }

  /** One directory per session: `<sessions>/<workspace slot>/<session id>/`. */
  async function scanSessionDirectories() {
    const found = []
    const slots = await fsp.readdir(sessionsRoot, { withFileTypes: true }).catch(() => [])
    for (const slot of slots) {
      if (!slot.isDirectory()) continue
      const slotPath = path.join(sessionsRoot, slot.name)
      const children = await fsp.readdir(slotPath, { withFileTypes: true }).catch(() => [])
      for (const child of children) {
        if (!child.isDirectory() || !SESSION_ID.test(child.name)) continue
        const sessionPath = path.join(slotPath, child.name)
        const stat = await fsp.stat(sessionPath).catch(() => null)
        found.push({
          id: child.name,
          slot: slot.name,
          modifiedAt: stat === null ? 0 : Math.round(stat.mtimeMs),
          bytes: await directorySize(sessionPath),
        })
      }
    }
    return found
  }

  async function describeSessions() {
    const scanned = await scanSessionDirectories()
    if (scanned.length === 0) return []

    const query = ctx.get('sessionQuery')
    const agents = ctx.get('agents')
    const store = ctx.get('sessions')
    const ids = scanned.map(entry => entry.id)
    const cwd = new Map()
    const titles = new Map()
    /** Loaded into memory at all, which includes sessions another reader merely prepared. */
    const loaded = new Set()

    if (query !== undefined) {
      const records = await query.listSessions().catch(() => [])
      for (const record of records) {
        const header = record.header
        if (header.cwd !== undefined) cwd.set(header.id, header.cwd)
        if (record.live) loaded.add(header.id)
      }
      const observations = await query.readTitleSnapshots(ids).catch(() => [])
      for (const observation of observations) {
        if (observation.status !== 'fulfilled') continue
        const title = observation.value.title
        if (title !== undefined && title.title.trim() !== '') titles.set(observation.sessionId, title.title)
      }
    }
    for (const entry of scanned) if (store !== undefined && store.get(entry.id) !== undefined) loaded.add(entry.id)

    return scanned
      .map(entry => ({
        id: entry.id,
        slot: entry.slot,
        title: titles.get(entry.id) ?? null,
        cwd: cwd.get(entry.id) ?? null,
        // Only a live Agent means the conversation is actually open: a session
        // merely loaded by this very list must stay deletable.
        live: agents !== undefined && agents.get(entry.id) !== undefined,
        loaded: loaded.has(entry.id),
        modifiedAt: entry.modifiedAt,
        bytes: entry.bytes,
      }))
      .sort((left, right) => right.modifiedAt - left.modifiedAt)
  }

  async function removeProjectionCache(sessionId) {
    const uuid = sessionId.replace(/^session-/, '')
    for (const name of new Set([`${sessionId}.json`, `${uuid}.json`])) {
      await fsp.rm(path.join(projectionCache, name), { force: true }).catch(() => {})
    }
  }

  async function isLiveSession(sessionId) {
    const agents = ctx.get('agents')
    return agents !== undefined && agents.get(sessionId) !== undefined
  }

  async function deleteSession(sessionId, force) {
    if (typeof sessionId !== 'string' || !SESSION_ID.test(sessionId)) {
      return { ok: false, status: 400, code: 'invalid-id' }
    }
    const live = await isLiveSession(sessionId)
    if (live && force !== true) {
      return { ok: false, status: 409, code: 'live' }
    }

    // Tell the workspace registry first, while the log still exists: an
    // archived id leaves the sidebar without pointing at a vanished log.
    // With `stopActivity` the archive is what releases a remembered Agent.
    const registry = ctx.get('workspaceRegistry')
    let archived = false
    if (registry !== undefined) {
      archived = await registry
        .archiveSession(sessionId, live ? { stopActivity: true } : undefined)
        .then(() => true, () => false)
    }
    if (live) {
      for (let attempt = 0; attempt < 25 && (await isLiveSession(sessionId)); attempt += 1) {
        await new Promise(resolve => setTimeout(resolve, 200))
      }
      if (await isLiveSession(sessionId)) {
        return { ok: false, status: 409, code: 'live', archived }
      }
    }

    let removed = 0
    let failure = null
    const slots = await fsp.readdir(sessionsRoot, { withFileTypes: true }).catch(() => [])
    for (const slot of slots) {
      if (!slot.isDirectory()) continue
      const target = path.join(sessionsRoot, slot.name, sessionId)
      const stat = await fsp.stat(target).catch(() => null)
      if (stat === null || !stat.isDirectory()) continue
      const done = await fsp.rm(target, { recursive: true, force: true }).then(
        () => true,
        error => {
          failure = String(error?.code ?? error?.message ?? error)
          return false
        },
      )
      if (done) removed += 1
    }

    await removeProjectionCache(sessionId)

    if (removed === 0) {
      return { ok: false, status: failure === null ? 404 : 423, code: failure === null ? 'missing' : 'locked', archived }
    }
    return { ok: true, status: 200, removed, archived }
  }

  function authorized(req) {
    if (req.headers[CSRF_HEADER] !== '1') return false
    const origin = req.headers.origin
    if (origin === undefined) return true
    return origin === ctx.webServer.getOrigin()
  }

  function route(resource) {
    return async (req, res) => {
      if (req.method !== 'POST') {
        res.writeHead(405, { allow: 'POST' })
        res.end()
        return
      }
      if (!authorized(req)) {
        send(res, 403, { ok: false, code: 'forbidden' })
        return
      }
      try {
        const result = await resource(req)
        send(res, result.status, result)
      } catch (error) {
        send(res, 500, { ok: false, code: 'failed', message: String(error?.message ?? error) })
      }
    }
  }

  const listRoute = route(async () => ({ ok: true, status: 200, sessions: await describeSessions() }))

  const deleteRoute = route(async req => {
    const body = await readJsonBody(req)
    if (body === null) return { ok: false, status: 400, code: 'bad-body' }
    return deleteSession(body.sessionId, body.force === true)
  })

  ctx.effect(() => {
    const disposers = [
      ctx.webServer.register({ kind: 'exact', path: LIST_PATH, handler: listRoute }),
      ctx.webServer.register({ kind: 'exact', path: DELETE_PATH, handler: deleteRoute }),
    ]
    return () => {
      for (const dispose of disposers) dispose()
    }
  })
}
