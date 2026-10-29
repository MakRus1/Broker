import { useEffect, useState, type FormEvent } from 'react';
import { Select } from './Select';
import { currenciesWithCurrent } from './currencies';
import { useFieldValidation } from './fieldValidation';
import { request, type Session } from './api';
import { useDiscardConfirmation } from './components';

export interface BankAccount {
  id: string;
  counterparty_id: string;
  name: string;
  number: string;
  currency: string;
  bank_name: string;
  bic?: string | null;
  swift?: string | null;
  correspondent_account?: string | null;
  iban?: string | null;
  is_primary: boolean;
  archived: boolean;
  revision: number;
}
const accountFields = [
  ['name', 'Название счёта', 256],
  ['number', 'Номер счёта', 64],
  ['currency', 'Валюта', 3],
  ['bank_name', 'Наименование банка', 256],
  ['bic', 'БИК российского банка', 9],
  ['swift', 'SWIFT / BIC иностранного банка', 11],
  ['correspondent_account', 'Корреспондентский счёт', 64],
  ['iban', 'IBAN', 34],
] as const;

export function BankAccounts({
  counterpartyId,
  session,
  archivedOwner,
  version,
  onError,
  onDirty,
}: {
  counterpartyId: string;
  session: Session;
  archivedOwner: boolean;
  version: number;
  onError: (error: unknown) => void;
  onDirty: (dirty: boolean) => void;
}) {
  const validation = useFieldValidation();
  const [items, setItems] = useState<BankAccount[]>([]);
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [refresh, setRefresh] = useState(0);
  const [showArchived, setShowArchived] = useState(false);
  const [editing, setEditing] = useState<BankAccount>();
  const [draft, setDraft] = useState<BankAccount>();
  const [busy, setBusy] = useState(false);
  const dirty = !!draft && JSON.stringify(draft) !== JSON.stringify(editing);
  const write =
    session.permissions.includes('counterparties.write_bank_accounts') && !archivedOwner;
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    onDirty(dirty || busy);
    return () => onDirty(false);
  }, [dirty, busy, onDirty]);
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    request<{ items: BankAccount[] }>(
      '/api/v1/counterparties/bank-accounts/list',
      session,
      { counterparty_id: counterpartyId },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setItems(result.items);
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error.message);
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [counterpartyId, session, version, refresh, onError]);
  function edit(value?: BankAccount) {
    const next = value ?? {
      id: crypto.randomUUID(),
      counterparty_id: counterpartyId,
      name: '',
      number: '',
      currency: 'RUB',
      bank_name: '',
      bic: null,
      swift: null,
      correspondent_account: null,
      iban: null,
      is_primary: false,
      archived: false,
      revision: 0,
    };
    validation.reset();
    setEditing(next);
    setDraft({ ...next });
    setFailure('');
  }
  function cancel() {
    if (!busy)
      confirmDiscard(dirty, () => {
        setDraft(undefined);
        setEditing(undefined);
        setFailure('');
      });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { revision, ...body } = draft;
      const account = Object.fromEntries(
        accountFields.map(([field]) => [
          field,
          ['name', 'number', 'currency', 'bank_name'].includes(field)
            ? (draft[field] ?? '').trim()
            : (draft[field] ?? '').trim() || null,
        ]),
      );
      await request('/api/v1/counterparties/bank-accounts/save', session, {
        ...body,
        ...account,
        expected_revision: revision,
      });
      setDraft(undefined);
      setEditing(undefined);
      setRefresh((value) => value + 1);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить счёт');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="bank-accounts">
      <div className="section-heading">
        <h2>Банковские счета</h2>
        {write && !draft && (
          <button className="primary" onClick={() => edit()}>
            Добавить счёт
          </button>
        )}
      </div>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {draft ? (
        <form
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <h3>{draft.revision ? 'Редактирование счёта' : 'Новый банковский счёт'}</h3>
          <fieldset disabled={busy} aria-label="Реквизиты счёта">
            <div className="route-fields">
              {accountFields.map(([field, label, maxLength]) =>
                field === 'currency' ? (
                  <Select
                    key={field}
                    label="Валюта"
                    value={draft.currency}
                    searchable
                    options={currenciesWithCurrent(draft.currency)}
                    disabled={busy}
                    onChange={(currency) => setDraft({ ...draft, currency })}
                  />
                ) : (
                  <label key={field}>
                    {label}
                    <input
                      name={field}
                      value={draft[field] ?? ''}
                      maxLength={maxLength}
                      required={['name', 'number', 'bank_name'].includes(field)}
                      pattern={
                        field === 'bic'
                          ? '[0-9]{9}'
                          : field === 'swift'
                            ? '[A-Z0-9]{8}([A-Z0-9]{3})?'
                            : undefined
                      }
                      onChange={(event) =>
                        setDraft({
                          ...draft,
                          [field]:
                            field === 'swift'
                              ? event.target.value.toUpperCase()
                              : event.target.value,
                        })
                      }
                    />
                  </label>
                ),
              )}
            </div>
            <div className="role-options">
              <label>
                <input
                  type="checkbox"
                  checked={draft.is_primary}
                  disabled={draft.archived}
                  onChange={(event) => setDraft({ ...draft, is_primary: event.target.checked })}
                />
                Основной для этой валюты
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={draft.archived}
                  onChange={(event) =>
                    setDraft({
                      ...draft,
                      archived: event.target.checked,
                      is_primary: event.target.checked ? false : draft.is_primary,
                    })
                  }
                />
                В архиве
              </label>
            </div>
          </fieldset>
          <div className="form-actions">
            <button type="button" onClick={cancel} disabled={busy}>
              Отменить
            </button>
            <button className="primary" disabled={busy}>
              {busy ? 'Сохраняем…' : 'Сохранить'}
            </button>
          </div>
        </form>
      ) : (
        <>
          <label className="bank-archive-toggle">
            <input
              type="checkbox"
              checked={showArchived}
              onChange={(event) => setShowArchived(event.target.checked)}
            />
            Показать архивные
          </label>
          {loading ? (
            <p className="hint">Загружаем счета…</p>
          ) : (
            <div className="bank-account-list">
              {items
                .filter((item) => showArchived || !item.archived)
                .map((item) => (
                  <article key={item.id} className="bank-account-card">
                    <div className="section-heading">
                      <div>
                        <h3>{item.name}</h3>
                        <p className="hint">
                          {item.currency}
                          {item.is_primary ? ' · Основной' : ''}
                          {item.archived ? ' · В архиве' : ''}
                        </p>
                      </div>
                      {write && <button onClick={() => edit(item)}>Редактировать</button>}
                    </div>
                    <dl className="counterparty-details">
                      {accountFields
                        .filter(([field]) => !['name', 'currency'].includes(field))
                        .map(
                          ([field, label]) =>
                            item[field] && (
                              <div key={field}>
                                <dt>{label}</dt>
                                <dd>{item[field]}</dd>
                              </div>
                            ),
                        )}
                    </dl>
                  </article>
                ))}
              {!items.some((item) => showArchived || !item.archived) && (
                <p className="hint">
                  {items.length
                    ? 'Действующих банковских счетов нет.'
                    : 'Банковские счета пока не добавлены.'}
                </p>
              )}
            </div>
          )}
        </>
      )}
      {confirmation}
    </section>
  );
}
