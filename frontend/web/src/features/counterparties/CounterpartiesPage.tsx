import { CounterpartySettlements } from '../finance/settlements/CounterpartySettlements';
import { BankAccounts } from './BankAccounts';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import {
  initialCounterpartyQuery,
  restoreCounterpartyQuery,
} from '../registry/registryQueryPreferences';
import { useColumnFilters } from '../registry/RegistryColumnFilters';
import { filterRequest } from '../registry/registryFilters';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from '../registry/RegistryTable';
import type { CounterpartyRegistryCursor, CounterpartySort } from './counterpartyRegistry';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { useCallback, useEffect, useRef, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { CardNavigation } from '../../shared/ui/CardNavigation';
import { Dialog } from '../../shared/ui/Dialog';
import { CounterpartyHistory } from './CounterpartyHistory';
import { counterpartyRoles, type Counterparty } from './counterparties';
import { CounterpartyForm } from './CounterpartyForm';

type Props = {
  route: string;
  onTitle?: (title: string) => void;
  active?: boolean;
  session: Session;
  onError: (error: unknown) => void;
  onChanged: (value: Counterparty) => void;
  onSaved: (value: Counterparty) => void;
};
export function CounterpartiesPage({
  route,
  session,
  onError,
  onChanged,
  onSaved,
  onTitle,
  active = true,
}: Props) {
  const columns: RegistryColumn<Counterparty>[] = [
    {
      id: 'name',
      label: 'Наименование',
      width: 340,
      required: true,
      sort: 'name',
      render: (value) => (
        <a
          className="link"
          href={`#/counterparties/${value.id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {value.name}
        </a>
      ),
    },
    {
      id: 'country_code',
      label: 'Страна',
      width: 150,
      sort: 'country_code',
      render: (value) => value.country_code || '—',
    },
    {
      id: 'tax_id',
      label: 'Налоговый номер',
      width: 210,
      sort: 'tax_id',
      render: (value) => value.tax_id || '—',
    },
    {
      id: 'roles',
      label: 'Роли',
      width: 280,
      render: (value) => value.roles.map((role) => counterpartyRoles[role]).join(', ') || '—',
    },
    {
      id: 'own_company',
      label: 'Собственное юрлицо',
      width: 180,
      render: (value) => (value.own_company ? 'Да' : 'Нет'),
    },
    {
      id: 'archived',
      label: 'Состояние',
      width: 180,
      sort: 'archived',
      render: (value) => (value.archived ? 'В архиве' : 'Действующий'),
    },
  ];
  const columnState = useRegistryColumns(session.subject, 'counterparties', columns);
  const id = /^\/counterparties\/([a-f0-9-]{36})(?:\/edit)?$/i.exec(route)?.[1];
  const creating = route === '/counterparties/new';
  const editing = creating || route.endsWith('/edit');
  const [tab, setTab] = useState<'details' | 'history' | 'accounts' | 'settlements'>('details');
  const [accountsDirty, setAccountsDirty] = useState(false);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  function switchTab(next: 'details' | 'history' | 'accounts' | 'settlements') {
    confirmDiscard(accountsDirty, () => setTab(next));
  }
  const [historyVersion, setHistoryVersion] = useState(0);
  const [item, setItem] = useState<Counterparty>();
  const [failure, setFailure] = useState('');
  const [items, setItems] = useState<Counterparty[]>([]);
  const [registryFilters, setRegistryFilters] = useRegistryPreference(
    preferenceKey(session.subject, 'counterparties', 'query'),
    initialCounterpartyQuery,
    restoreCounterpartyQuery,
  );
  const { sort, descending } = registryFilters;
  const filters = useColumnFilters({
    registry: 'counterparties',
    session,
    query: registryFilters,
    onChange: (filters) => setRegistryFilters((old) => ({ ...old, filters })),
    onSort: (sort, descending) =>
      setRegistryFilters((old) => ({ ...old, sort: sort as CounterpartySort, descending })),
  });
  const [nextCursor, setNextCursor] = useState<CounterpartyRegistryCursor>();
  const [more, setMore] = useState(false);
  const [loading, setLoading] = useState(true);
  const [archiving, setArchiving] = useState(false);
  const [archiveFailure, setArchiveFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const epoch = useRef(0);
  useEffect(() => {
    if (item) onTitle?.(item.name);
  }, [item?.name]);
  const write = session.permissions.includes('counterparties.write');
  const read = session.permissions.includes('counterparties.read');
  const load = useCallback(
    async (cursor?: CounterpartyRegistryCursor, signal?: AbortSignal) => {
      const current = ++epoch.current;
      if (!cursor) {
        setItems([]);
        setMore(false);
      }
      setLoading(true);
      setFailure('');
      try {
        const result = await request<{
          items: Counterparty[];
          next_cursor?: CounterpartyRegistryCursor;
        }>(
          '/api/v1/counterparties/list',
          session,
          {
            filters: filterRequest(registryFilters.filters),
            sort,
            descending,
            limit: 50,
            ...(cursor ? { cursor } : {}),
          },
          signal,
        );
        if (signal?.aborted || current !== epoch.current) return;
        setItems((old) => (cursor ? [...old, ...result.items] : result.items));
        setNextCursor(result.next_cursor);
        setMore(Boolean(result.next_cursor));
      } catch (error) {
        if (!signal?.aborted && current === epoch.current) {
          setFailure(error instanceof Error ? error.message : 'Ошибка загрузки');
          onError(error);
        }
      } finally {
        if (current === epoch.current) setLoading(false);
      }
    },
    [sort, descending, registryFilters.filters, session, onError],
  );
  useEffect(() => {
    if (id || creating || !read) return;
    const controller = new AbortController();
    setLoading(true);
    void load(undefined, controller.signal);
    return () => {
      controller.abort();
      epoch.current++;
    };
  }, [id, creating, read, load]);
  const refresh = useCallback(
    async (signal?: AbortSignal) => {
      if (!id) return;
      const result = await request<{ counterparty: Counterparty }>(
        '/api/v1/counterparties/get',
        session,
        { id },
        signal,
      );
      if (!signal?.aborted) {
        setItem(result.counterparty);
        setHistoryVersion((old) => old + 1);
        setFailure('');
      }
    },
    [id, session],
  );
  useEffect(() => {
    setItem(undefined);
    setTab('details');
    setFailure('');
    if (!id || !read) return;
    const controller = new AbortController();
    void refresh(controller.signal).catch((error) => {
      if (!controller.signal.aborted) {
        setFailure(error.message);
        onError(error);
      }
    });
    return () => controller.abort();
  }, [id, read, refresh, onError]);
  useEffect(() => {
    if (active)
      document.title = `${creating ? 'Новый контрагент' : item?.name || 'Контрагенты'} · Broker`;
  }, [creating, item?.name]);
  async function archive() {
    if (!item) return;
    setBusy(true);
    setArchiveFailure('');
    try {
      const result = await request<{ counterparty: Counterparty }>(
        '/api/v1/counterparties/set-archived',
        session,
        { id: item.id, expected_revision: item.revision, archived: !item.archived },
      );
      setItem(result.counterparty);
      onChanged(result.counterparty);
      setArchiving(false);
    } catch (error) {
      setArchiveFailure(error instanceof Error ? error.message : 'Не удалось изменить состояние');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  if (!read)
    return (
      <section className="creation-page surface">
        <h1>Контрагенты</h1>
        <p>Нет права на просмотр справочника.</p>
      </section>
    );
  if (creating || id)
    return (
      <section className="creation-page surface counterparty-page">
        <a className="link" href="#/counterparties">
          ← К контрагентам
        </a>
        {failure && (
          <p role="alert" className="error">
            {failure}
          </p>
        )}
        {id && item?.id !== id ? (
          <p>{failure ? 'Карточка недоступна.' : 'Открываем контрагента…'}</p>
        ) : editing ? (
          !write || item?.archived ? (
            <p>Редактирование недоступно. Архивного контрагента сначала нужно восстановить.</p>
          ) : (
            <CounterpartyForm
              key={creating ? 'new' : item?.id}
              initial={creating ? undefined : item}
              session={session}
              onError={onError}
              onSaved={(value) => {
                setItem(value);
                onChanged(value);
                onSaved(value);
              }}
            />
          )
        ) : (
          item && (
            <>
              <div className="page-heading counterparty-heading">
                <div>
                  <p className="eyebrow">
                    {item.own_company ? 'СОБСТВЕННОЕ ЮРЛИЦО' : 'КОНТРАГЕНТ · ЮРИДИЧЕСКОЕ ЛИЦО'}
                  </p>
                  <h1>{item.name}</h1>
                  <p>
                    {item.archived ? 'В архиве' : 'Действующий'} · Редакция {item.revision}
                  </p>
                </div>
                <div className="actions">
                  {write && !item.archived && (
                    <a className="primary" href={`#/counterparties/${item.id}/edit`}>
                      Редактировать
                    </a>
                  )}
                  {write && (
                    <button
                      onClick={() => {
                        setArchiveFailure('');
                        setArchiving(true);
                      }}
                    >
                      {item.archived ? 'Восстановить' : 'В архив'}
                    </button>
                  )}
                </div>
              </div>
              {item.archived && (
                <p className="route-notice">
                  Контрагент скрыт из нового выбора. Связанные поставки, перевозки и история
                  сохранены.
                </p>
              )}
              <CardNavigation onRefresh={() => refresh().catch(onError)}>
                <button aria-pressed={tab === 'details'} onClick={() => switchTab('details')}>
                  Реквизиты
                </button>
                {session.permissions.includes('counterparties.read_bank_accounts') && (
                  <button aria-pressed={tab === 'accounts'} onClick={() => switchTab('accounts')}>
                    Банковские счета
                  </button>
                )}
                {session.permissions.includes('finance.read_invoices') &&
                  session.permissions.includes('finance.read_payments') && (
                    <button
                      aria-pressed={tab === 'settlements'}
                      onClick={() => switchTab('settlements')}
                    >
                      Взаиморасчёты
                    </button>
                  )}
                <button aria-pressed={tab === 'history'} onClick={() => switchTab('history')}>
                  История
                </button>
              </CardNavigation>
              {confirmation}
              {tab === 'settlements' ? (
                <CounterpartySettlements
                  key={item.id}
                  counterpartyId={item.id}
                  counterpartyName={item.name}
                  session={session}
                  version={historyVersion}
                />
              ) : tab === 'accounts' ? (
                <BankAccounts
                  counterpartyId={item.id}
                  session={session}
                  archivedOwner={item.archived}
                  version={historyVersion}
                  onError={onError}
                  onDirty={setAccountsDirty}
                />
              ) : tab === 'history' ? (
                <CounterpartyHistory
                  key={item.id}
                  id={item.id}
                  session={session}
                  onError={onError}
                  version={historyVersion + item.revision}
                />
              ) : (
                <>
                  {item.roles.length > 0 && (
                    <div className="counterparty-roles">
                      {item.roles.map((value) => (
                        <span className="badge" key={value}>
                          {counterpartyRoles[value]}
                        </span>
                      ))}
                    </div>
                  )}
                  <dl className="counterparty-details">
                    {[
                      ['Юридическое наименование', item.legal_name],
                      ['Страна', item.country_code],
                      ['Налоговый номер / ИНН', item.tax_id],
                      ['КПП', item.kpp],
                      ['Регистрационный номер / ОГРН', item.registration_id],
                      ['Юридический адрес', item.legal_address],
                    ].map(([label, value]) => (
                      <div key={label}>
                        <dt>{label}</dt>
                        <dd>{value || 'Не указано'}</dd>
                      </div>
                    ))}
                    <div>
                      <dt>Логист</dt>
                      <dd>
                        <EmployeeName subject={item.logist} session={session} />
                      </dd>
                    </div>
                  </dl>
                  <section className="counterparty-contacts">
                    <h2>Контактные лица</h2>
                    {!item.contacts.length ? (
                      <p className="hint">Контакты пока не добавлены.</p>
                    ) : (
                      <div className="table-scroll">
                        <table>
                          <thead>
                            <tr>
                              <th>Имя</th>
                              <th>Должность</th>
                              <th>Email</th>
                              <th>Телефон</th>
                            </tr>
                          </thead>
                          <tbody>
                            {item.contacts.map((contact, index) => (
                              <tr key={index}>
                                <td>{contact.name}</td>
                                <td>{contact.position || '—'}</td>
                                <td>{contact.email || '—'}</td>
                                <td>{contact.phone || '—'}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    )}
                  </section>
                </>
              )}
              {archiving && (
                <Dialog
                  title={
                    item.archived ? 'Восстановить контрагента?' : 'Отправить контрагента в архив?'
                  }
                  onClose={() => {
                    if (!busy) setArchiving(false);
                  }}
                >
                  <form
                    data-busy={busy}
                    onSubmit={(event) => {
                      event.preventDefault();
                      void archive();
                    }}
                  >
                    <p>
                      {item.archived
                        ? 'Контрагент снова появится при выборе клиентов и подрядчиков.'
                        : 'Он исчезнет из нового выбора. Все существующие связи останутся.'}
                    </p>
                    {archiveFailure && (
                      <p className="error" role="alert">
                        {archiveFailure}
                      </p>
                    )}
                    <div className="confirmation-actions">
                      <button type="button" disabled={busy} onClick={() => setArchiving(false)}>
                        Вернуться
                      </button>
                      <button className="primary" disabled={busy}>
                        {busy ? 'Сохраняем…' : item.archived ? 'Восстановить' : 'В архив'}
                      </button>
                    </div>
                  </form>
                </Dialog>
              )}
            </>
          )
        )}
      </section>
    );
  return (
    <div className="counterparty-registry">
      <div className="page-heading">
        <div>
          <h1>Контрагенты</h1>
          <span className="registry-count" role="status">
            {loading ? 'Загрузка…' : `Загружено: ${items.length}`}
          </span>
        </div>
        <div className="actions">
          <RegistryColumnSettings {...columnState} />
          <button disabled={loading} onClick={() => void load()}>
            Обновить
          </button>
          {write && (
            <a className="primary" href="#/counterparties/new">
              ＋ Новый контрагент
            </a>
          )}
        </div>
      </div>
      <section className="surface counterparty-list" aria-busy={loading}>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <RegistryTable
          items={items}
          columns={columnState}
          definitions={columns}
          filters={filters}
          sort={sort}
          descending={descending}
          onSort={(field) => {
            setRegistryFilters((old) => ({
              ...old,
              sort: field as CounterpartySort,
              descending: old.sort === field ? !old.descending : false,
            }));
          }}
          onOpen={onSaved}
        />
        {loading && (
          <p className="directory-loading" role="status">
            Загружаем контрагентов…
          </p>
        )}
        {!items.length && !loading && (
          <div className="empty">
            <h2>Контрагенты не найдены</h2>
            <p>Измените поиск или создайте новую карточку.</p>
          </div>
        )}
        <AutoLoadMore
          enabled={more}
          loading={loading}
          failed={Boolean(failure)}
          onLoad={() => load(nextCursor)}
        />
      </section>
    </div>
  );
}
