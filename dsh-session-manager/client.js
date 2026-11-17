/**
 * Client half of the session-manager bundle.
 *
 * Registers the left sidebar entry (`sidebar.panellist`) and the central panel
 * it opens (`main`, keyed by the same id). The panel only reads and deletes
 * through the Host half's two JSON routes; the durable session store is never
 * touched from the browser.
 */
window.__ModuleLoader__.load({
  id: 'dsh-session-manager',
  factory(require) {
    const React = require('react')
    const h = React.createElement

    const NS = 'session-manager'
    const LIST_ROUTE = '/plugin-session-manager/list'
    const DELETE_ROUTE = '/plugin-session-manager/delete'
    const PANEL_ID = 'session-manager'
    const CSRF_HEADERS = {
      'content-type': 'application/json',
      'x-dsh-session-manager': '1',
    }

    const DICT_EN = {
      'panel.title': 'Session manager',
      'panel.subtitle':
        'Every session saved on this machine. Deleting one removes its log and its sidebar entry for good.',
      'action.refresh': 'Refresh',
      'action.delete': 'Delete',
      'action.cancel': 'Cancel',
      'action.confirm': 'Delete for good',
      'filter.placeholder': 'Filter by title or folder',
      'count.all': '{count} sessions',
      'count.matching': '{count} shown',
      'badge.live': 'Open',
      'hint.live': 'Still open in this app; deleting stops it first.',
      'action.confirmLive': 'Stop and delete',
      'state.loading': 'Loading sessions…',
      'state.empty': 'No saved sessions.',
      'state.noMatch': 'No session matches this filter.',
      'state.untitled': 'Untitled session',
      'state.unknownFolder': 'Unknown folder',
      'confirm.title': 'Delete permanently?',
      'confirm.body': 'The log is removed from disk and cannot be restored.',
      'confirm.live': 'Still open: it is stopped first, then deleted for good.',
      'notice.deleted': 'Session deleted.',
      'error.live': 'This session is still open and could not be released.',
      'error.locked': 'Its log file is still held open; close the app and delete it again.',
      'error.invalid-id': 'That session id is not valid.',
      'error.missing': 'The session log is already gone.',
      'error.forbidden': 'This page is not allowed to change sessions.',
      'error.list-failed': 'Sessions could not be read.',
      'error.delete-failed': 'The session could not be deleted.',
      'error.network': 'The Harness is unreachable.',
      'error.unknown': 'Something went wrong.',
    }

    const DICT_ZH = {
      'panel.title': '会话管理',
      'panel.subtitle': '列出本机保存的全部会话。删除会移除会话日志与侧边栏条目，无法恢复。',
      'action.refresh': '刷新',
      'action.delete': '删除',
      'action.cancel': '取消',
      'action.confirm': '永久删除',
      'filter.placeholder': '按标题或目录筛选',
      'count.all': '共 {count} 个会话',
      'count.matching': '显示 {count} 个',
      'badge.live': '打开中',
      'hint.live': '仍在运行中的应用里打开；删除会先停止它。',
      'action.confirmLive': '停止并删除',
      'state.loading': '正在读取会话…',
      'state.empty': '没有已保存的会话。',
      'state.noMatch': '没有符合条件的会话。',
      'state.untitled': '未命名会话',
      'state.unknownFolder': '未知目录',
      'confirm.title': '确认永久删除？',
      'confirm.body': '日志会从磁盘移除，无法恢复。',
      'confirm.live': '仍在打开：先停止它，再永久删除。',
      'notice.deleted': '会话已删除。',
      'error.live': '该会话仍在运行中，未能释放。',
      'error.locked': '日志文件仍被占用；关闭应用后再删一次。',
      'error.invalid-id': '会话 ID 无效。',
      'error.missing': '会话日志已不存在。',
      'error.forbidden': '当前页面无权修改会话。',
      'error.list-failed': '无法读取会话列表。',
      'error.delete-failed': '会话删除失败。',
      'error.network': '无法连接 Harness。',
      'error.unknown': '操作失败。',
    }

    const CSS = `
.sm-root { box-sizing: border-box; height: 100%; min-height: 0; overflow: auto; background: var(--dsw-alias-bg-base); color: var(--dsw-alias-label-primary); }
.sm-inner { max-width: 920px; margin: 0 auto; padding: 28px 24px 56px; display: flex; flex-direction: column; gap: 16px; }
.sm-head { display: flex; align-items: flex-start; justify-content: space-between; gap: 16px; }
.sm-title { margin: 0; font-size: 19px; font-weight: 600; line-height: 26px; }
.sm-sub { margin: 6px 0 0; font-size: 13px; line-height: 20px; color: var(--dsw-alias-label-secondary); max-width: 640px; }
.sm-toolbar { display: flex; align-items: center; gap: 10px; }
.sm-input { flex: 1 1 auto; min-width: 0; height: 32px; padding: 0 10px; font: inherit; font-size: 13px; color: var(--dsw-alias-label-primary); background: var(--dsw-alias-bg-layer-1); border: 1px solid var(--dsw-alias-border-l1); border-radius: 8px; outline: none; }
.sm-input:focus { border-color: var(--dsw-alias-brand-primary); }
.sm-input::placeholder { color: var(--dsw-alias-label-secondary); }
.sm-count { font-size: 12px; color: var(--dsw-alias-label-secondary); white-space: nowrap; }
.sm-list { list-style: none; margin: 0; padding: 0; border: 1px solid var(--dsw-alias-border-l1); border-radius: 10px; overflow: hidden; background: var(--dsw-alias-bg-layer-1); }
.sm-row { display: flex; align-items: center; gap: 12px; padding: 12px 14px; border-top: 1px solid var(--dsw-alias-border-l1); }
.sm-row:first-child { border-top: none; }
.sm-main { min-width: 0; flex: 1 1 auto; display: flex; flex-direction: column; gap: 3px; }
.sm-name { font-size: 13px; font-weight: 500; line-height: 19px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.sm-name.is-untitled { color: var(--dsw-alias-label-secondary); font-weight: 400; }
.sm-meta { font-size: 12px; line-height: 17px; color: var(--dsw-alias-label-secondary); overflow: hidden; white-space: nowrap; text-overflow: ellipsis; }
.sm-btn { height: 30px; padding: 0 12px; font: inherit; font-size: 13px; border-radius: 8px; border: 1px solid var(--dsw-alias-border-l1); background: transparent; color: var(--dsw-alias-label-primary); cursor: pointer; white-space: nowrap; }
.sm-btn:hover:not(:disabled) { background: var(--dsw-alias-bg-layer-2); }
.sm-btn:disabled { opacity: .45; cursor: default; }
.sm-btn.is-danger { color: var(--dsw-alias-state-error-primary); border-color: var(--dsw-alias-state-error-primary); }
.sm-btn.is-quiet { color: var(--dsw-alias-label-secondary); }
.sm-badge { font-size: 11px; line-height: 18px; padding: 0 7px; border-radius: 999px; border: 1px solid var(--dsw-alias-state-success-primary); color: var(--dsw-alias-state-success-primary); white-space: nowrap; }
.sm-confirm { display: flex; align-items: center; gap: 8px; }
.sm-confirm-label { font-size: 12px; color: var(--dsw-alias-state-error-primary); white-space: nowrap; }
.sm-note { padding: 10px 12px; border-radius: 8px; font-size: 13px; line-height: 19px; border: 1px solid var(--dsw-alias-border-l1); background: var(--dsw-alias-bg-layer-2); color: var(--dsw-alias-label-primary); }
.sm-note.is-error { border-color: var(--dsw-alias-state-error-primary); color: var(--dsw-alias-state-error-primary); }
.sm-empty { padding: 26px 16px; text-align: center; font-size: 13px; color: var(--dsw-alias-label-secondary); }
`

    function formatBytes(bytes) {
      if (!Number.isFinite(bytes) || bytes <= 0) return null
      if (bytes < 1024) return `${bytes} B`
      if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
      return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
    }

    function formatTime(value, locale) {
      if (!Number.isFinite(value) || value <= 0) return null
      try {
        return new Date(value).toLocaleString(locale, { hour12: false })
      } catch {
        return new Date(value).toISOString().slice(0, 16).replace('T', ' ')
      }
    }

    async function call(url, body) {
      let response
      try {
        response = await fetch(url, {
          method: 'POST',
          headers: CSRF_HEADERS,
          body: JSON.stringify(body ?? {}),
        })
      } catch {
        throw new Error('network')
      }
      const payload = await response.json().catch(() => null)
      const ok = payload !== null && payload.ok === true
      if (!response.ok || !ok) throw new Error((payload && payload.code) || `http-${response.status}`)
      return payload
    }

    function apply(ctx) {
      const t = ctx.locale.bind(NS)
      const locale = ctx.locale

      ctx.effect(() => {
        const disposers = [
          locale.register(NS, 'en', DICT_EN),
          locale.register(NS, 'zh', DICT_ZH),
          locale.register(NS, 'zh-CN', DICT_ZH),
        ]
        return () => {
          for (const dispose of disposers) dispose()
        }
      })

      /** Re-render on locale switch or dictionary arrival; `t` reads the active locale when called. */
      function useTranslate() {
        React.useSyncExternalStore(
          React.useCallback(fn => locale.subscribe(fn), []),
          React.useCallback(() => locale.getSnapshot(), []),
          React.useCallback(() => locale.getSnapshot(), []),
        )
        return t
      }

      function message(code) {
        const key = `error.${code}`
        const text = t(key)
        return text === key ? `${t('error.unknown')} (${code})` : text
      }

      function fill(key, count) {
        return t(key).replace('{count}', String(count))
      }

      function PanelIcon(props) {
        const size = props !== null && typeof props === 'object' && typeof props.size === 'number' ? props.size : 18
        const active = props !== null && typeof props === 'object' && props.active === true
        return h(
          'svg',
          {
            width: size,
            height: size,
            viewBox: '0 0 24 24',
            fill: 'none',
            'aria-hidden': 'true',
            style: { display: 'block', opacity: active ? 1 : 0.85 },
          },
          h('path', {
            d: 'M4 7h16',
            stroke: 'currentColor',
            strokeWidth: 1.6,
            strokeLinecap: 'round',
          }),
          h('path', {
            d: 'M9.2 7V5.6A1.6 1.6 0 0 1 10.8 4h2.4a1.6 1.6 0 0 1 1.6 1.6V7',
            stroke: 'currentColor',
            strokeWidth: 1.6,
            strokeLinecap: 'round',
          }),
          h('path', {
            d: 'M6.6 7l.8 11.2a1.6 1.6 0 0 0 1.6 1.5h6a1.6 1.6 0 0 0 1.6-1.5L17.4 7',
            stroke: 'currentColor',
            strokeWidth: 1.6,
            strokeLinejoin: 'round',
          }),
          h('path', {
            d: 'M10.3 10.8v5.4M13.7 10.8v5.4',
            stroke: 'currentColor',
            strokeWidth: 1.6,
            strokeLinecap: 'round',
          }),
        )
      }

      function Panel() {
        const translate = useTranslate()
        const [sessions, setSessions] = React.useState([])
        const [status, setStatus] = React.useState('loading')
        const [error, setError] = React.useState(null)
        const [notice, setNotice] = React.useState(null)
        const [filter, setFilter] = React.useState('')
        const [confirming, setConfirming] = React.useState(null)
        const [busy, setBusy] = React.useState(null)

        const load = React.useCallback(async () => {
          setStatus('loading')
          setError(null)
          try {
            const payload = await call(LIST_ROUTE, {})
            setSessions(Array.isArray(payload.sessions) ? payload.sessions : [])
            setStatus('ready')
          } catch (failure) {
            setStatus('failed')
            setError(message(failure.message))
          }
        }, [])

        React.useEffect(() => {
          void load()
        }, [load])

        const remove = React.useCallback(async (id, force) => {
          setBusy(id)
          setError(null)
          setNotice(null)
          try {
            await call(DELETE_ROUTE, { sessionId: id, force: force === true })
            setSessions(current => current.filter(entry => entry.id !== id))
            setConfirming(null)
            setNotice(translate('notice.deleted'))
          } catch (failure) {
            setError(message(failure.message))
          } finally {
            setBusy(null)
          }
        }, [])

        const localeId = locale.getLocale().active
        const shown = React.useMemo(() => {
          const needle = filter.trim().toLowerCase()
          if (needle === '') return sessions
          return sessions.filter(entry => {
            const title = (entry.title ?? '').toLowerCase()
            const folder = (entry.cwd ?? entry.slot ?? '').toLowerCase()
            return title.includes(needle) || folder.includes(needle)
          })
        }, [sessions, filter])

        const listBody = (() => {
          if (status === 'loading') return h('div', { className: 'sm-empty' }, translate('state.loading'))
          if (status === 'failed') return null
          if (sessions.length === 0) return h('div', { className: 'sm-empty' }, translate('state.empty'))
          if (shown.length === 0) return h('div', { className: 'sm-empty' }, translate('state.noMatch'))
          return h(
            'ul',
            { className: 'sm-list' },
            shown.map(entry => {
              const untitled = entry.title === null || entry.title === undefined || entry.title === ''
              const parts = [entry.cwd ?? translate('state.unknownFolder')]
              const time = formatTime(entry.modifiedAt, localeId)
              if (time !== null) parts.push(time)
              const size = formatBytes(entry.bytes)
              if (size !== null) parts.push(size)
              const pending = confirming === entry.id
              return h(
                'li',
                { className: 'sm-row', key: entry.id },
                h(
                  'div',
                  { className: 'sm-main' },
                  h(
                    'div',
                    { className: 'sm-name' + (untitled ? ' is-untitled' : ''), title: entry.title ?? entry.id },
                    untitled ? translate('state.untitled') : entry.title,
                  ),
                  h('div', { className: 'sm-meta', title: parts.join(' · ') }, parts.join(' · ')),
                ),
                entry.live === true
                  ? h('span', { className: 'sm-badge', title: translate('hint.live') }, translate('badge.live'))
                  : null,
                pending
                  ? h(
                      'div',
                      { className: 'sm-confirm' },
                      h(
                        'span',
                        { className: 'sm-confirm-label', title: entry.live === true ? translate('confirm.live') : translate('confirm.body') },
                        entry.live === true ? translate('confirm.live') : translate('confirm.title'),
                      ),
                      h(
                        'button',
                        {
                          type: 'button',
                          className: 'sm-btn is-danger',
                          disabled: busy === entry.id,
                          onClick: () => void remove(entry.id, entry.live === true),
                        },
                        entry.live === true ? translate('action.confirmLive') : translate('action.confirm'),
                      ),
                      h(
                        'button',
                        {
                          type: 'button',
                          className: 'sm-btn is-quiet',
                          onClick: () => setConfirming(null),
                        },
                        translate('action.cancel'),
                      ),
                    )
                  : h(
                      'button',
                      {
                        type: 'button',
                        className: 'sm-btn is-danger',
                        disabled: busy === entry.id,
                        title: entry.live === true ? translate('hint.live') : translate('confirm.body'),
                        onClick: () => {
                          setNotice(null)
                          setConfirming(entry.id)
                        },
                      },
                      translate('action.delete'),
                    ),
              )
            }),
          )
        })()

        return h(
          'div',
          { className: 'sm-root' },
          h('style', null, CSS),
          h(
            'div',
            { className: 'sm-inner' },
            h(
              'header',
              { className: 'sm-head' },
              h(
                'div',
                null,
                h('h1', { className: 'sm-title' }, translate('panel.title')),
                h('p', { className: 'sm-sub' }, translate('panel.subtitle')),
              ),
              h(
                'button',
                {
                  type: 'button',
                  className: 'sm-btn',
                  onClick: () => void load(),
                  disabled: status === 'loading',
                },
                translate('action.refresh'),
              ),
            ),
            h(
              'div',
              { className: 'sm-toolbar' },
              h('input', {
                className: 'sm-input',
                type: 'search',
                value: filter,
                placeholder: translate('filter.placeholder'),
                'aria-label': translate('filter.placeholder'),
                onChange: event => setFilter(event.target.value),
              }),
              h(
                'span',
                { className: 'sm-count' },
                fill(filter.trim() === '' ? 'count.all' : 'count.matching', shown.length),
              ),
            ),
            error === null ? null : h('div', { className: 'sm-note is-error', role: 'status' }, error),
            notice === null ? null : h('div', { className: 'sm-note', role: 'status' }, notice),
            listBody,
          ),
        )
      }

      ctx.slots.inject('sidebar.panellist', () =>
        ctx.slots.register(
          {
            name: 'sidebar.panellist',
            id: PANEL_ID,
            order: 40,
            label: () => t('panel.title'),
          },
          PanelIcon,
        ),
      )

      ctx.slots.inject('main', () =>
        ctx.slots.register({ name: 'main', key: PANEL_ID }, Panel),
      )
    }

    return { inject: ['slots', 'locale'], apply }
  },
})
