import { useEffect, useState } from 'react';
import {
  ApiError,
  canApprove,
  parseValue,
  request,
  type CatalogEntry,
  type Current,
  type Draft,
  type DraftEvent,
  type Session,
} from '../api';
const pretty = (value: unknown) => JSON.stringify(value, null, 2);

export function useWorkspace() {
  const [session, setSession] = useState<Session>();
  const [loginRequired, setLoginRequired] = useState(false);
  const [catalog, setCatalog] = useState<CatalogEntry[]>([]);
  const [selected, setSelected] = useState('');
  const [current, setCurrent] = useState<Current>();
  const [drafts, setDrafts] = useState<Draft[]>([]);
  const [draft, setDraft] = useState<Draft>();
  const [events, setEvents] = useState<DraftEvent[]>([]);
  const [editor, setEditor] = useState(false);
  const [text, setText] = useState('');
  const [description, setDescription] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [notice, setNotice] = useState('');
  const write = session?.permissions.includes('configs.write');
  const owner = draft?.author === session?.actor && draft?.status === 'draft';

  function fail(reason: unknown) {
    if (reason instanceof ApiError && reason.status === 401) {
      setSession(undefined);
      setLoginRequired(true);
    }
    setError(reason instanceof Error ? reason.message : 'Не удалось выполнить запрос');
  }
  async function refreshCatalog() {
    const value = await request<{ items: CatalogEntry[] }>('/api/configs/list');
    setCatalog(value.items);
  }
  useEffect(() => {
    request<Session>('/api/session')
      .then(async (value) => {
        setSession(value);
        await refreshCatalog();
      })
      .catch(fail);
  }, []);
  useEffect(() => {
    if (!selected || !session) return;
    let cancelled = false;
    setCurrent(undefined);
    setDraft(undefined);
    setEditor(false);
    setEvents([]);
    setError('');
    setNotice('');
    Promise.all([
      request<Current>('/api/configs/current', session, { config_name: selected }),
      request<{ items: Draft[] }>('/api/configs/drafts/list', session, { config_name: selected }),
    ])
      .then(([value, history]) => {
        if (cancelled) return;
        setCurrent(value);
        setDrafts(history.items);
        if (value.active_draft_id)
          setDraft(history.items.find((item) => item.id === value.active_draft_id));
      })
      .catch((reason) => {
        if (!cancelled) fail(reason);
      });
    return () => {
      cancelled = true;
    };
  }, [selected, session]);
  useEffect(() => {
    if (!draft || !session) {
      setEvents([]);
      return;
    }
    let cancelled = false;
    request<{ items: DraftEvent[] }>('/api/configs/drafts/events', session, { id: draft.id })
      .then((value) => {
        if (!cancelled) setEvents(value.items);
      })
      .catch((reason) => {
        if (!cancelled) fail(reason);
      });
    return () => {
      cancelled = true;
    };
  }, [draft, session]);

  async function run(action: () => Promise<void>) {
    setBusy(true);
    setError('');
    setNotice('');
    try {
      await action();
    } catch (reason) {
      fail(reason);
    } finally {
      setBusy(false);
    }
  }
  function edit() {
    setText(pretty(owner && draft ? draft.value : current?.value));
    setDescription(owner && draft ? draft.description : '');
    setEditor(true);
    setError('');
    setNotice('');
  }
  async function reload() {
    if (!session || !selected) return;
    const [value, history] = await Promise.all([
      request<Current>('/api/configs/current', session, { config_name: selected }),
      request<{ items: Draft[] }>('/api/configs/drafts/list', session, { config_name: selected }),
    ]);
    setCurrent(value);
    setDrafts(history.items);
    await refreshCatalog();
    if (draft) setDraft(history.items.find((item) => item.id === draft.id));
  }
  async function save() {
    if (!session || !current) return;
    const value = parseValue(text);
    if (!description.trim()) throw new Error('Опишите, зачем меняется конфиг');
    const result =
      owner && draft
        ? await request<Draft>('/api/configs/drafts/edit', session, {
            id: draft.id,
            expected_version: draft.version,
            value,
            description,
          })
        : await request<Draft>('/api/configs/drafts/create', session, {
            config_name: selected,
            expected_config_version: current.version,
            value,
            description,
          });
    setDraft(result);
    setEditor(false);
    setNotice('Драфт сохранён. Для публикации требуется подтверждение другого пользователя.');
    const fresh = await request<Current>('/api/configs/current', session, {
      config_name: selected,
    });
    setCurrent(fresh);
    await refreshCatalog();
    setDrafts(
      (
        await request<{ items: Draft[] }>('/api/configs/drafts/list', session, {
          config_name: selected,
        })
      ).items,
    );
  }
  async function transition(operation: 'approve' | 'cancel') {
    if (!session || !draft) return;
    const value = await request<Draft>('/api/configs/drafts/' + operation, session, {
      id: draft.id,
      expected_version: draft.version,
    });
    setDraft(value);
    setEditor(false);
    setNotice(operation === 'approve' ? 'Конфиг опубликован.' : 'Драфт отменён.');
    const [fresh, history] = await Promise.all([
      request<Current>('/api/configs/current', session, { config_name: selected }),
      request<{ items: Draft[] }>('/api/configs/drafts/list', session, { config_name: selected }),
    ]);
    setCurrent(fresh);
    setDrafts(history.items);
    await refreshCatalog();
  }
  async function logout() {
    if (!session) return;
    const value = await request<{ logout_url: string }>('/api/logout', session, {});
    location.assign(value.logout_url);
  }
  return {
    session,
    loginRequired,
    catalog,
    selected,
    current,
    drafts,
    draft,
    events,
    editor,
    text,
    description,
    busy,
    error,
    notice,
    write,
    owner,
    setSelected,
    setDraft,
    setEditor,
    setText,
    setDescription,
    run,
    edit,
    save,
    transition,
    reload,
    logout,
  };
}

export type Workspace = ReturnType<typeof useWorkspace>;
