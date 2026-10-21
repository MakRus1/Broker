import { useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { Dialog } from './components';

export interface Counterparty {
  id: string;
  name: string;
}

export function CustomerSelect({
  items,
  value,
  onChange,
  onCreated,
  session,
  disabled,
}: {
  items: Counterparty[];
  value: string;
  onChange: (id: string) => void;
  onCreated: (item: Counterparty) => void;
  session: Session;
  disabled: boolean;
}) {
  const [creating, setCreating] = useState(false);
  return (
    <>
      <label>
        Клиент
        <select
          required
          disabled={disabled}
          value={value}
          onChange={(event) => onChange(event.target.value)}
        >
          <option value="">Выберите клиента</option>
          {items.map((item) => (
            <option key={item.id} value={item.id}>
              {item.name}
            </option>
          ))}
        </select>
      </label>
      {session.permissions.includes('counterparties.write') && (
        <button type="button" disabled={disabled} onClick={() => setCreating(true)}>
          ＋ Новый клиент
        </button>
      )}
      {creating && (
        <Dialog title="Новый клиент" onClose={() => setCreating(false)}>
          <CustomerForm
            session={session}
            onCreated={(item) => {
              onCreated(item);
              onChange(item.id);
              setCreating(false);
            }}
          />
        </Dialog>
      )}
    </>
  );
}

function CustomerForm({
  session,
  onCreated,
}: {
  session: Session;
  onCreated: (item: Counterparty) => void;
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
        { id, name: name.trim() },
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
