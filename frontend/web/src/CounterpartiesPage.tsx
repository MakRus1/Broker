import { Select } from './Select';
import { AutoLoadMore } from './AutoLoadMore';
import { useCallback, useEffect, useRef, useState } from 'react';
import { request, type Session } from './api';
import { CardNavigation, Dialog } from './components';
import { CounterpartyHistory } from './CounterpartyHistory';
import { counterpartyRoles, type Counterparty } from './counterparties';
import { CounterpartyForm } from './CounterpartyForm';

type Props = {
  route: string;
  session: Session;
  onError: (error: unknown) => void;
  onChanged: (value: Counterparty) => void;
};
export function CounterpartiesPage({ route, session, onError, onChanged }: Props) {
  const id = /^\/counterparties\/([a-f0-9-]{36})(?:\/edit)?$/i.exec(route)?.[1];
  const creating = route === '/counterparties/new';
  const editing = creating || route.endsWith('/edit');
  const [tab, setTab] = useState<'details' | 'history'>('details');
  const [historyVersion, setHistoryVersion] = useState(0);
  const [item, setItem] = useState<Counterparty>();
  const [failure, setFailure] = useState('');
  const [items, setItems] = useState<Counterparty[]>([]);
  const [query, setQuery] = useState('');
  const [role, setRole] = useState('');
  const [status, setStatus] = useState('active');
  const [more, setMore] = useState(false);
  const [loading, setLoading] = useState(true);
  const [archiving, setArchiving] = useState(false);
  const [archiveFailure, setArchiveFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const epoch = useRef(0);
  const write = session.permissions.includes('counterparties.write');
  const read = session.permissions.includes('counterparties.read');
  const load = useCallback(
    async (after?: string, signal?: AbortSignal) => {
      const current = ++epoch.current;
      setLoading(true);
      setFailure('');
      try {
        const result = await request<{ items: Counterparty[] }>(
          '/api/v1/counterparties/list',
          session,
          { query, status, ...(role ? { role } : {}), limit: 50, ...(after ? { after } : {}) },
          signal,
        );
        if (signal?.aborted || current !== epoch.current) return;
        setItems((old) => (after ? [...old, ...result.items] : result.items));
        setMore(result.items.length === 50);
      } catch (error) {
        if (!signal?.aborted && current === epoch.current) {
          setFailure(error instanceof Error ? error.message : 'Ошибка загрузки');
          onError(error);
        }
      } finally {
        if (current === epoch.current) setLoading(false);
      }
    },
    [query, role, status, session, onError],
  );
  useEffect(() => {
    if (id || creating || !read) return;
    const controller = new AbortController();
    setLoading(true);
    const timer = setTimeout(() => void load(undefined, controller.signal), 250);
    return () => {
      clearTimeout(timer);
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
                location.hash = `/counterparties/${value.id}`;
              }}
            />
          )
        ) : (
          item && (
            <>
              <div className="page-heading counterparty-heading">
                <div>
                  <p className="eyebrow">КОНТРАГЕНТ · ЮРИДИЧЕСКОЕ ЛИЦО</p>
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
                <button aria-pressed={tab === 'details'} onClick={() => setTab('details')}>
                  Реквизиты
                </button>
                <button aria-pressed={tab === 'history'} onClick={() => setTab('history')}>
                  История
                </button>
              </CardNavigation>
              {tab === 'history' ? (
                <CounterpartyHistory
                  key={item.id}
                  id={item.id}
                  session={session}
                  onError={onError}
                  version={historyVersion + item.revision}
                />
              ) : (
                <>
                  <div className="counterparty-roles">
                    {item.roles.map((value) => (
                      <span className="badge" key={value}>
                        {counterpartyRoles[value]}
                      </span>
                    ))}
                    {!item.roles.length && <span className="hint">Роли пока не назначены</span>}
                  </div>
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
                  </dl>
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
    <>
      <div className="page-heading">
        <div>
          <p className="eyebrow">СПРАВОЧНИК</p>
          <h1>Контрагенты</h1>
          <p>Юридические лица, их реквизиты, контакты и роли.</p>
        </div>
        <div className="actions">
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
        <div className="directory-filters">
          <label>
            Поиск
            <input
              type="search"
              value={query}
              maxLength={256}
              placeholder="Название или налоговый номер"
              onChange={(event) => setQuery(event.target.value)}
            />
          </label>
          <Select
            label="Роль"
            value={role}
            onChange={setRole}
            options={[
              { value: '', label: 'Все роли' },
              ...Object.entries(counterpartyRoles).map(([value, label]) => ({ value, label })),
            ]}
          />
          <Select
            label="Состояние"
            value={status}
            onChange={setStatus}
            options={[
              { value: 'active', label: 'Действующие' },
              { value: 'archived', label: 'Архив' },
              { value: 'all', label: 'Все' },
            ]}
          />
        </div>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="table-scroll">
          <table>
            <thead>
              <tr>
                <th>Наименование</th>
                <th>Страна</th>
                <th>Налоговый номер</th>
                <th>Роли</th>
                <th>Состояние</th>
              </tr>
            </thead>
            <tbody>
              {items.map((value) => (
                <tr
                  key={value.id}
                  className="clickable"
                  onClick={() => {
                    location.hash = `/counterparties/${value.id}`;
                  }}
                >
                  <td>
                    <a
                      className="link"
                      href={`#/counterparties/${value.id}`}
                      onClick={(event) => event.stopPropagation()}
                    >
                      {value.name}
                    </a>
                  </td>
                  <td>{value.country_code || '—'}</td>
                  <td>{value.tax_id || '—'}</td>
                  <td>{value.roles.map((role) => counterpartyRoles[role]).join(', ') || '—'}</td>
                  <td>{value.archived ? 'В архиве' : 'Действующий'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
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
          onLoad={() => load(items.at(-1)?.id)}
        />
      </section>
    </>
  );
}
