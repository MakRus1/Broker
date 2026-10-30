import { BankAccountSelect } from './BankAccountSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { useColumnFilters } from './RegistryColumnFilters';
import { preferenceKey, useRegistryPreference } from './registryPreferences';
import { initialContractQuery, restoreContractQuery } from './registryQueryPreferences';
import { filterRequest } from './registryFilters';
import { type BankAccount } from './BankAccounts';
import { Select } from './Select';
import { currenciesWithCurrent } from './currencies';
import { CalendarDate } from './CalendarDate';
import { DateInput } from './DateInput';
import { AutoLoadMore } from './AutoLoadMore';
import { useDiscardConfirmation } from './components';
import { useFieldValidation } from './fieldValidation';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from './RegistryTable';

const emptyParties: Counterparty[] = [];
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };

export interface Contract {
  id: string;
  own_company_id: string;
  counterparty_id: string;
  number: string;
  contract_date: string;
  valid_until: string | null;
  currency: string;
  own_bank_account_id: string | null;
  counterparty_bank_account_id: string | null;
  payment_delay_days: number | null;
  note: string | null;
  archived: boolean;
  revision: number;
}
type Draft = Omit<Contract, 'contract_date' | 'valid_until'> & {
  contract_date: CalendarDate | null;
  valid_until: CalendarDate | null;
};
const date = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
function newContract(): Draft {
  const now = new Date();
  return {
    id: crypto.randomUUID(),
    own_company_id: '',
    counterparty_id: '',
    number: '',
    contract_date: CalendarDate.fromISO(
      `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`,
    ),
    valid_until: null,
    currency: 'RUB',
    own_bank_account_id: null,
    counterparty_bank_account_id: null,
    payment_delay_days: null,
    note: null,
    archived: false,
    revision: 0,
  };
}
function PartyDetails({
  id,
  accountId,
  label,
  session,
}: {
  id: string;
  accountId: string | null;
  label: string;
  session: Session;
}) {
  const [name, setName] = useState('Загружаем…');
  const [account, setAccount] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    request<{ counterparty: { name: string } }>(
      '/api/v1/counterparties/get',
      session,
      { id },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setName(result.counterparty.name);
      })
      .catch(() => {
        if (!controller.signal.aborted) setName('Компания недоступна');
      });
    if (accountId && session.permissions.includes('counterparties.read_bank_accounts'))
      request<{ items: BankAccount[] }>(
        '/api/v1/counterparties/bank-accounts/list',
        session,
        { counterparty_id: id },
        controller.signal,
      )
        .then((result) => {
          if (!controller.signal.aborted) {
            const item = result.items.find((item) => item.id === accountId);
            setAccount(
              item ? `${item.name} · ${item.number} · ${item.bank_name}` : 'Счёт недоступен',
            );
          }
        })
        .catch(() => {
          if (!controller.signal.aborted) setAccount('Счёт недоступен');
        });
    return () => controller.abort();
  }, [id, accountId, session]);
  return (
    <div>
      <span className="hint">{label}</span>
      <p>
        <a className="link" href={`#/counterparties/${id}`}>
          {name}
        </a>
      </p>
      <p className="hint">
        {accountId
          ? account ||
            (session.permissions.includes('counterparties.read_bank_accounts')
              ? 'Загружаем счёт…'
              : 'Нет права на просмотр счёта')
          : 'Банковский счёт не выбран'}
      </p>
    </div>
  );
}

export function ContractsPage({
  route,
  session,
  onError,
  onTitle,
  onSaved,
  onCancelNew,
  active,
}: {
  active: boolean;
  route: string;
  session: Session;
  onError: (error: unknown) => void;
  onTitle: (title: string) => void;
  onSaved: (value: Contract) => void;
  onCancelNew: () => void;
}) {
  const titleCallback = useRef(onTitle);
  titleCallback.current = onTitle;
  const id = /^\/contracts\/([0-9a-f-]{36})/i.exec(route)?.[1];
  const creating = route === '/contracts/new';
  const editing = creating || route.endsWith('/edit');
  const [value, setValue] = useState<Contract>();
  const [draft, setDraft] = useState<Draft>();
  const initialDraft = useRef('');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [items, setItems] = useState<Contract[]>([]);
  const [after, setAfter] = useState<Cursor>();
  const [registryQuery, setRegistryQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'contracts', 'query'),
    initialContractQuery,
    restoreContractQuery,
  );
  const columnFilters = useColumnFilters({
    registry: 'contracts',
    session,
    query: registryQuery,
    onChange: (filters) => setRegistryQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setRegistryQuery((old) => ({ ...old, sort, descending })),
  });
  const [partyNames, setPartyNames] = useState<Record<string, string>>({});
  const names = useRef<Record<string, string>>({});
  const [namesFailed, setNamesFailed] = useState(false);
  useEffect(() => {
    const missing = [
      ...new Set(items.flatMap((item) => [item.own_company_id, item.counterparty_id])),
    ].filter((id) => !(id in names.current));
    if (!missing.length) return;
    setNamesFailed(false);
    const controller = new AbortController();
    void (async () => {
      for (let start = 0; start < missing.length; start += 100) {
        const found = await request<{ items: Counterparty[] }>(
          '/api/v1/counterparties/list',
          session,
          { filters: [{ field: 'id', values: missing.slice(start, start + 100) }], limit: 100 },
          controller.signal,
        );
        if (controller.signal.aborted) return;
        for (const id of missing.slice(start, start + 100))
          names.current[id] = 'Недоступная компания';
        for (const party of found.items) names.current[party.id] = party.name;
        setPartyNames({ ...names.current });
      }
    })().catch(() => {
      if (!controller.signal.aborted) setNamesFailed(true);
    });
    return () => controller.abort();
  }, [items, session]);
  const [loading, setLoading] = useState(false);
  const [version, setVersion] = useState(0);
  const validation = useFieldValidation();
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const read = session.permissions.includes('contracts.read');
  const write = session.permissions.includes('contracts.write');
  useEffect(() => {
    if (creating) {
      setDraft((old) => {
        if (old) return old;
        const next = newContract();
        initialDraft.current = JSON.stringify(next);
        return next;
      });
      setValue(undefined);
      return;
    }
    if (!id || !read) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    request<{ contract: Contract }>('/api/v1/contracts/get', session, { id }, controller.signal)
      .then((result) => {
        if (controller.signal.aborted) return;
        setValue(result.contract);
        setDraft({
          ...result.contract,
          contract_date: CalendarDate.fromISO(result.contract.contract_date),
          valid_until: result.contract.valid_until
            ? CalendarDate.fromISO(result.contract.valid_until)
            : null,
        });
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
  }, [id, creating, session, read, version]);
  useEffect(() => {
    titleCallback.current(
      creating ? 'Новый договор' : (value?.number ?? (id ? 'Договор' : 'Договоры')),
    );
  }, [creating, value?.number]);
  useEffect(() => {
    if (!active || !read || id || creating) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setAfter(undefined);
    request<{ items: Contract[]; next_cursor?: Cursor }>(
      '/api/v1/contracts/list',
      session,
      { ...registryQuery, filters: filterRequest(registryQuery.filters), limit: 50 },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setAfter(result.next_cursor);
        }
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
  }, [id, creating, read, active, registryQuery, session, version]);
  const columns: RegistryColumn<Contract>[] = [
    {
      id: 'number',
      label: 'Договор',
      width: 240,
      required: true,
      sort: 'number',
      render: (item) => (
        <a
          href={`#/contracts/${item.id}`}
          className="link"
          onClick={(event) => event.stopPropagation()}
        >
          {item.number}
        </a>
      ),
    },
    {
      id: 'contract_date',
      sort: 'contract_date',
      label: 'Дата договора',
      width: 180,
      render: (item) => date(item.contract_date),
    },
    {
      id: 'valid_until',
      sort: 'valid_until',
      label: 'Действует до',
      width: 180,
      render: (item) => date(item.valid_until),
    },
    {
      id: 'own_company',
      label: 'Собственное юрлицо',
      width: 280,
      filterFields: ['own_company_id'],
      render: (item) =>
        partyNames[item.own_company_id] ?? (namesFailed ? 'Недоступно' : 'Загружаем…'),
    },
    {
      id: 'counterparty',
      label: 'Контрагент',
      width: 280,
      filterFields: ['counterparty_id'],
      render: (item) =>
        partyNames[item.counterparty_id] ?? (namesFailed ? 'Недоступно' : 'Загружаем…'),
    },
    {
      id: 'currency',
      label: 'Валюта',
      width: 140,
      sort: 'currency',
      render: (item) => item.currency,
    },
    {
      id: 'archived',
      label: 'Состояние',
      width: 180,
      sort: 'archived',
      render: (item) => (item.archived ? 'В архиве' : 'Действующий'),
    },
    {
      id: 'payment_delay_days',
      label: 'Отсрочка, дней',
      width: 180,
      render: (item) => item.payment_delay_days ?? 'Не указана',
    },
  ];
  const columnState = useRegistryColumns(session.subject, 'contracts', columns);
  const dirty =
    editing &&
    !!draft &&
    JSON.stringify(draft) !== (creating ? initialDraft.current : JSON.stringify(value));
  useEffect(() => {
    const leaving = (event: BeforeUnloadEvent) => {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    };
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [dirty, busy]);

  if (!read)
    return (
      <section className="card">
        <h1>Договоры</h1>
        <p>Нет права на просмотр договоров.</p>
      </section>
    );

  function cancel() {
    confirmDiscard(dirty, () => {
      if (creating) onCancelNew();
      else {
        if (value)
          setDraft({
            ...value,
            contract_date: CalendarDate.fromISO(value.contract_date),
            valid_until: value.valid_until ? CalendarDate.fromISO(value.valid_until) : null,
          });
        location.hash = `/contracts/${id}`;
      }
    });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { revision, ...body } = draft;
      const result = await request<{ contract: Contract }>('/api/v1/contracts/save', session, {
        ...body,
        expected_revision: revision,
      });
      setValue(result.contract);
      setDraft({
        ...result.contract,
        contract_date: CalendarDate.fromISO(result.contract.contract_date),
        valid_until: result.contract.valid_until
          ? CalendarDate.fromISO(result.contract.valid_until)
          : null,
      });
      onSaved(result.contract);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить договор');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  async function more() {
    if (!after || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const result = await request<{ items: Contract[]; next_cursor?: Cursor }>(
        '/api/v1/contracts/list',
        session,
        {
          ...registryQuery,
          filters: filterRequest(registryQuery.filters),
          limit: 50,
          cursor: after,
        },
      );
      setItems((old) => [...old, ...result.items]);
      setAfter(result.next_cursor);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить договоры');
      onError(error);
    } finally {
      setLoading(false);
    }
  }
  if (!id && !creating)
    return (
      <section className="card record-page">
        <div className="section-heading">
          <h1>Договоры</h1>
          {write && (
            <a className="button primary" href="#/contracts/new">
              Новый договор
            </a>
          )}
        </div>
        <div className="registry-toolbar">
          <RegistryColumnSettings {...columnState} />
        </div>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <RegistryTable
          items={items}
          emptyMessage={
            loading
              ? 'Загружаем договоры…'
              : failure
                ? undefined
                : registryQuery.filters.length > 1
                  ? 'Нет договоров по выбранным условиям'
                  : 'Пока нет договоров'
          }
          columns={columnState}
          filters={columnFilters}
          sort={registryQuery.sort}
          descending={registryQuery.descending}
          onSort={(sort) =>
            setRegistryQuery((old) => ({
              ...old,
              sort,
              descending: old.sort === sort ? !old.descending : false,
            }))
          }
          definitions={columns}
          onOpen={(item) => {
            location.hash = `/contracts/${item.id}`;
          }}
        />
        <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
      </section>
    );
  return (
    <section className="card record-page">
      {confirmation}
      <div className="record-heading">
        <a className="link record-back" href="#/contracts">
          ← К договорам
        </a>
        <div className="section-heading">
          <div>
            {!creating && <p className="eyebrow">ДОГОВОР</p>}
            <h1>{creating ? 'Новый договор' : (value?.number ?? 'Договор')}</h1>
            {value && !editing && (
              <p className="record-state">
                {value.archived ? 'В архиве' : 'Действующий'} · Редакция {value.revision}
              </p>
            )}
          </div>
          {!editing && value && write && (
            <a className="button primary" href={`#/contracts/${id}/edit`}>
              Редактировать
            </a>
          )}
        </div>
      </div>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {loading && <p className="hint">Загружаем договор…</p>}
      {editing && draft && write && !loading ? (
        <form
          className="counterparty-form"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <div className="record-fields">
            <label>
              Номер договора · обязательно
              <input
                required
                name="number"
                maxLength={128}
                value={draft.number}
                onChange={(event) => setDraft({ ...draft, number: event.target.value })}
              />
            </label>
            <label>
              Дата договора · обязательно
              <DateInput
                required
                name="contract_date"
                min={CalendarDate.fromISO('1900-01-01')}
                value={draft.contract_date}
                onChange={(contract_date) => setDraft({ ...draft, contract_date })}
              />
            </label>
            <CustomerSelect
              items={emptyParties}
              label="Собственное юрлицо · обязательно"
              required
              ownCompany
              session={session}
              disabled={busy}
              value={draft.own_company_id}
              onChange={(own_company_id) =>
                setDraft({ ...draft, own_company_id, own_bank_account_id: null })
              }
            />
            <CustomerSelect
              items={emptyParties}
              label="Контрагент · обязательно"
              required
              session={session}
              disabled={busy}
              value={draft.counterparty_id}
              onChange={(counterparty_id) =>
                setDraft({ ...draft, counterparty_id, counterparty_bank_account_id: null })
              }
            />
            <div>
              <Select
                label="Валюта"
                value={draft.currency}
                onChange={(currency) =>
                  setDraft({
                    ...draft,
                    currency,
                    own_bank_account_id: null,
                    counterparty_bank_account_id: null,
                  })
                }
                searchable
                options={currenciesWithCurrent(draft.currency)}
              />
            </div>
            <label>
              Действует до
              <DateInput
                value={draft.valid_until}
                min={draft.contract_date ?? CalendarDate.fromISO('1900-01-01')}
                onChange={(valid_until) => setDraft({ ...draft, valid_until })}
              />
            </label>
            <div>
              <BankAccountSelect
                label="Наш банковский счёт"
                session={session}
                owner={draft.own_company_id}
                currency={draft.currency}
                value={draft.own_bank_account_id}
                onChange={(own_bank_account_id) => setDraft({ ...draft, own_bank_account_id })}
              />
            </div>
            <div>
              <BankAccountSelect
                label="Счёт контрагента"
                session={session}
                owner={draft.counterparty_id}
                currency={draft.currency}
                value={draft.counterparty_bank_account_id}
                onChange={(counterparty_bank_account_id) =>
                  setDraft({ ...draft, counterparty_bank_account_id })
                }
              />
            </div>
            <label>
              Отсрочка, дней
              <input
                type="number"
                min={0}
                max={36500}
                step={1}
                value={draft.payment_delay_days ?? ''}
                onChange={(event) =>
                  setDraft({
                    ...draft,
                    payment_delay_days:
                      event.target.value === '' ? null : Number(event.target.value),
                  })
                }
              />
            </label>
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={draft.archived}
                onChange={(event) => setDraft({ ...draft, archived: event.target.checked })}
              />
              В архиве
            </label>
            <label className="record-wide">
              Примечание
              <textarea
                rows={3}
                maxLength={4000}
                value={draft.note ?? ''}
                onChange={(event) => setDraft({ ...draft, note: event.target.value || null })}
              />
            </label>
          </div>
          <div className="form-actions">
            <button type="button" disabled={busy} onClick={cancel}>
              Отменить
            </button>
            <button className="primary" disabled={busy}>
              {busy ? 'Сохраняем…' : 'Сохранить'}
            </button>
          </div>
        </form>
      ) : (
        value &&
        !editing && (
          <>
            <div className="record-fields record-summary">
              <div>
                <span className="hint">Дата договора</span>
                <p>{date(value.contract_date)}</p>
              </div>
              <div>
                <span className="hint">Действует до</span>
                <p>{date(value.valid_until)}</p>
              </div>
              <PartyDetails
                label="Собственное юрлицо"
                id={value.own_company_id}
                accountId={value.own_bank_account_id}
                session={session}
              />
              <PartyDetails
                label="Контрагент"
                id={value.counterparty_id}
                accountId={value.counterparty_bank_account_id}
                session={session}
              />
              <div>
                <span className="hint">Валюта</span>
                <p>{value.currency}</p>
              </div>
              <div>
                <span className="hint">Отсрочка</span>
                <p>
                  {value.payment_delay_days === null
                    ? 'Не указана'
                    : `${value.payment_delay_days} дн.`}
                </p>
              </div>
              <div className="record-wide">
                <span className="hint">Примечание</span>
                <p>{value.note || '—'}</p>
              </div>
            </div>
          </>
        )
      )}
    </section>
  );
}
