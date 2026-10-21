import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { Dialog } from './components';

export type { Counterparty } from './counterparties';
import type { Counterparty } from './counterparties';

export function CustomerSelect({
  items,
  value,
  onChange,
  onCreated,
  session,
  disabled,
  label = 'Клиент',
}: {
  items: Counterparty[];
  value: string;
  onChange: (id: string) => void;
  onCreated: (item: Counterparty) => void;
  session: Session;
  disabled: boolean;
  label?: string;
}) {
  const [creating, setCreating] = useState(false);
  const [query, setQuery] = useState('');
  const [results, setResults] = useState<Counterparty[]>([]);
  const [selected, setSelected] = useState<Counterparty>();
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  useEffect(() => {
    if (!session.permissions.includes('counterparties.read')) return;
    const controller = new AbortController();
    const timer = setTimeout(() => {
      setLoading(true);
      void request<{ items: Counterparty[] }>(
        '/api/v1/counterparties/list',
        session,
        { query, limit: 100, status: 'active' },
        controller.signal,
      )
        .then((result) => {
          if (!controller.signal.aborted) {
            setResults(result.items);
            setFailure('');
          }
        })
        .catch((error) => {
          if (!controller.signal.aborted) setFailure(error.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 250);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [query, session]);
  useEffect(() => {
    setSelected((old) => (old?.id === value ? old : undefined));
    if (!value || items.some((item) => item.id === value)) return;
    const controller = new AbortController();
    void request<{ counterparty: Counterparty }>(
      '/api/v1/counterparties/get',
      session,
      { id: value },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setSelected(result.counterparty);
      })
      .catch(() => {
        /* Недоступную связанную компанию показываем без её данных. */
      });
    return () => controller.abort();
  }, [value, items, session]);
  const current = items.find((item) => item.id === value) ?? selected;
  const options = [...results];
  if (current && !options.some((item) => item.id === current.id)) options.unshift(current);

  return (
    <div className="counterparty-picker">
      <label>
        Поиск контрагента
        <input
          type="search"
          disabled={disabled}
          value={query}
          maxLength={256}
          placeholder="Название или налоговый номер"
          onChange={(event) => setQuery(event.target.value)}
        />
      </label>
      <label>
        {label}
        <select
          required={label === 'Клиент'}
          disabled={disabled}
          value={value}
          onChange={(event) => onChange(event.target.value)}
        >
          <option value="">
            {loading
              ? 'Поиск…'
              : query.trim() && !results.length
                ? 'Совпадений нет'
                : label === 'Клиент'
                  ? 'Выберите контрагента'
                  : 'Не выбран'}
          </option>
          {value && !current && !results.some((item) => item.id === value) && (
            <option value={value} disabled>
              Контрагент недоступен
            </option>
          )}
          {options
            .filter((item) => !item.archived || item.id === value)
            .map((item) => (
              <option key={item.id} value={item.id} disabled={item.archived}>
                {item.name}
                {item.archived ? ' · архив' : ''}
              </option>
            ))}
        </select>
      </label>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <p className="hint">
        Показано до 100 совпадений. Для уточнения используйте поиск; доступны все роли.
      </p>
      {session.permissions.includes('counterparties.write') && (
        <button type="button" disabled={disabled} onClick={() => setCreating(true)}>
          ＋ Новый контрагент
        </button>
      )}
      {creating && (
        <Dialog title="Новый контрагент" onClose={() => setCreating(false)}>
          <CustomerForm
            session={session}
            role={label === 'Подрядчик' ? 'carrier' : 'customer'}
            onCreated={(item) => {
              setSelected(item);
              setResults((old) => [item, ...old.filter((value) => value.id !== item.id)]);
              onCreated(item);
              onChange(item.id);
              setCreating(false);
            }}
          />
        </Dialog>
      )}
    </div>
  );
}

function CustomerForm({
  session,
  onCreated,
  role,
}: {
  session: Session;
  onCreated: (item: Counterparty) => void;
  role: 'customer' | 'carrier';
}) {
  const [name, setName] = useState('');
  // Идентификатор сохраняется при повторе запроса после сетевой ошибки.
  const [id] = useState(() => crypto.randomUUID());
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  async function submit(event: FormEvent) {
    event.preventDefault();
    event.stopPropagation();
    setBusy(true);
    setError('');
    try {
      const result = await request<{ counterparty: Counterparty }>(
        '/api/v1/counterparties/create',
        session,
        { id, name: name.trim(), roles: [role] },
      );
      onCreated(result.counterparty);
    } catch (value) {
      setError(value instanceof Error ? value.message : 'Не удалось создать клиента');
    } finally {
      setBusy(false);
    }
  }
  return (
    <form onSubmit={submit} data-dirty={Boolean(name)} data-busy={busy}>
      <label>
        Наименование
        <input
          autoFocus
          required
          maxLength={256}
          disabled={busy}
          value={name}
          onChange={(event) => setName(event.target.value)}
        />
      </label>
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      <button className="primary" disabled={busy || !name.trim()}>
        {busy ? 'Сохраняем…' : 'Создать клиента'}
      </button>
    </form>
  );
}
