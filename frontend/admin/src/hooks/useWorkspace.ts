import { useEffect, useState } from 'react';
import { editorState, hasUnsavedChanges } from '../changes';
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
  const [creating, setCreating] = useState(false);
  const [removing, setRemoving] = useState(false);
  const [newName, setNewName] = useState('');
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
  const [discardAction, setDiscardAction] = useState<(() => void) | null>(null);
  const write = session?.permissions.includes('configs.write');
  const owner = draft?.author === session?.actor && draft?.status === 'draft';

  function fail(reason: unknown) {
    if (reason instanceof ApiError && reason.status === 401) {
      setSession(undefined);
      setLoginRequired(true);
    }
    setError(
      reason instanceof ApiError && reason.status === 400
        ? `Данные не прошли проверку Configs: ${reason.message}. Проверьте значение и обязательные поля.`
        : reason instanceof ApiError && reason.status === 409
          ? 'Конфиг или драфт уже изменился. Обновите данные и повторите действие.'
          : reason instanceof Error
            ? reason.message
            : 'Не удалось выполнить запрос',
    );
  }
  async function refreshCatalog() {
    const value = await request<{ items: CatalogEntry[] }>('/api/configs/list');
    setCatalog(value.items);
  }
  useEffect(() => {
    let active = true;
    function refreshSession() {
      request<Session>('/api/session')
        .then(async (value) => {
          if (!active) return;
          setSession(value);
          if (value.permissions.includes('configs.read')) await refreshCatalog();
          else setCatalog([]);
        })
        .catch((reason: unknown) => {
          if (active) fail(reason);
        });
    }
    refreshSession();
    window.addEventListener('broker-permissions-changed', refreshSession);
    return () => {
      active = false;
      window.removeEventListener('broker-permissions-changed', refreshSession);
    };
  }, []);
  useEffect(() => {
    if (creating || !selected || !session || !session.permissions.includes('configs.read')) return;
    let cancelled = false;
    setCurrent(undefined);
    setRemoving(false);
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
  }, [selected, session, creating]);
  useEffect(() => {
    if (!draft || !session || !session.permissions.includes('configs.read')) {
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
  function closeConfig() {
    setCreating(false);
    setRemoving(false);
    setSelected('');
    setEditor(false);
  }
  function createConfig() {
    setSelected('');
    setCreating(true);
    setRemoving(false);
    setNewName('');
    setCurrent({ config_name: '', version: 0, value: null });
    setDraft(undefined);
    setDrafts([]);
    setEvents([]);
    setText('{}');
    setDescription('');
    setEditor(true);
    setError('');
    setNotice('');
  }
  function removeConfig() {
    if (!current || current.value === null) return;
    setRemoving(true);
    setText(pretty(current.value));
    setDescription('');
    setEditor(true);
    setError('');
    setNotice('');
  }
  function edit() {
    setRemoving(false);
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
  function restore(value: Current['value'], label: string) {
    setRemoving(false);
    setDraft(drafts.find((item) => item.id === current?.active_draft_id));
    setText(pretty(value));
    setDescription(label);
    setEditor(true);
  }
  const initial = creating ? {} : owner && draft ? draft.value : current?.value;
  const editing = editorState(text, initial, current?.value);
  const saveDisabled =
    busy ||
    !description.trim() ||
    !!editing.error ||
    (creating
      ? !/^[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*$/.test(newName) || newName.length > 256
      : !removing && !editing.changed);
  const unsaved =
    editor &&
    ((creating && newName.length > 0) ||
      hasUnsavedChanges(text, initial, description, owner && draft ? draft.description : ''));
  function discardChanges(action: () => void) {
    if (unsaved) setDiscardAction(() => action);
    else action();
  }
  function keepEditing() {
    setDiscardAction(null);
  }
  function confirmDiscard() {
    setDiscardAction(null);
    discardAction?.();
  }
  async function save() {
    if (!session || !current) return;
    if (editing.error) throw new Error(editing.error);
    if (saveDisabled) return;
    const name = creating ? newName : selected;
    let expectedVersion = current.version;
    if (creating) {
      const existing = await request<Current>('/api/configs/current', session, {
        config_name: name,
      });
      if (existing.value !== null || existing.active_draft_id)
        throw new Error('Конфиг с таким именем уже существует. Откройте его в каталоге.');
      expectedVersion = existing.version;
    }
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
            config_name: name,
            expected_config_version: expectedVersion,
            ...(removing ? { remove: true } : { value }),
            description,
          });
    setCreating(false);
    setRemoving(false);
    setSelected(name);
    setDraft(result);
    setEditor(false);
    setNotice('Драфт сохранён. Для публикации требуется подтверждение другого пользователя.');
    const fresh = await request<Current>('/api/configs/current', session, {
      config_name: name,
    });
    setCurrent(fresh);
    await refreshCatalog();
    setDrafts(
      (
        await request<{ items: Draft[] }>('/api/configs/drafts/list', session, {
          config_name: name,
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
    if (operation === 'cancel' && current?.value === null) {
      closeConfig();
      await refreshCatalog();
      return;
    }
    setDraft(value);
    setEditor(false);
    setNotice(
      operation === 'approve'
        ? draft.value === null
          ? 'Конфиг удалён.'
          : 'Конфиг опубликован.'
        : 'Драфт отменён.',
    );
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
    removing,
    removeConfig,
    creating,
    newName,
    setNewName,
    createConfig,
    closeConfig,
    editing,
    confirmDiscard,
    discardChanges,
    discardPending: discardAction !== null,
    keepEditing,
    saveDisabled,
    restore,
    setError,
    setNotice,
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
