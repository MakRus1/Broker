import { openWorkspace, replaceWorkspace, workspaceId, type WorkspaceTab } from './workspace';
import { ShipmentWorkspace } from './ShipmentWorkspace';
import { useDiscardConfirmation } from './components';
import { profileName } from './profile';
import { AutoLoadMore } from './AutoLoadMore';
import { useCallback, useEffect, useRef, useState } from 'react';
import { ApiError, request, type Session, type Shipment, type Transportation } from './api';
import { CounterpartiesPage } from './CounterpartiesPage';
import { type Counterparty } from './CustomerSelect';
import { TransportationPage } from './TransportationPage';
import { NewTransportation } from './TransportationDetailsForm';
import { TransportationTable } from './components';

export function App() {
  const [session, setSession] = useState<Session | null>();
  const [tab, setTab] = useState<'shipments' | 'transportations' | 'counterparties'>('shipments');
  const [customers, setCustomers] = useState<Counterparty[]>([]);
  const [shipments, setShipments] = useState<Shipment[]>([]);
  const [transportations, setTransportations] = useState<Transportation[]>([]);

  const [route, setRoute] = useState(() => location.hash.slice(1) || '/shipments');
  const creating = /^\/shipments\/new(?:\/|$)/.test(route);
  const [workspaces, setWorkspaces] = useState<WorkspaceTab[]>(() => openWorkspace([], route));
  const activeWorkspace = workspaceId(route);
  const workspaceNodes = useRef(new Map<string, HTMLDivElement>());
  const workspaceScroll = useRef(new Map<string, number>());
  const lastWorkspace = useRef(activeWorkspace);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    setWorkspaces((old) => openWorkspace(old, route));
  }, [route]);
  useEffect(() => {
    const old = lastWorkspace.current;
    if (old !== activeWorkspace) {
      workspaceScroll.current.set(old, window.scrollY);
      window.scrollTo(0, workspaceScroll.current.get(activeWorkspace) ?? 0);
      lastWorkspace.current = activeWorkspace;
    }
  }, [activeWorkspace]);
  const renameWorkspace = useCallback((id: string, title: string) => {
    setWorkspaces((old) =>
      old.map((tab) => (tab.id === id && tab.title !== title ? { ...tab, title } : tab)),
    );
  }, []);
  function closeWorkspace(id: string) {
    const node = workspaceNodes.current.get(id);
    if (node?.querySelector('form[data-busy="true"]')) return;
    confirmDiscard(Boolean(node?.querySelector('form[data-dirty="true"]')), () => {
      node?.querySelectorAll<HTMLElement>('[data-draft-key]').forEach((form) => {
        try {
          sessionStorage.removeItem(form.dataset.draftKey!);
        } catch {
          /* Закрытие формы не зависит от хранилища. */
        }
      });
      const index = workspaces.findIndex((tab) => tab.id === id);
      const remaining = workspaces.filter((tab) => tab.id !== id);
      setWorkspaces(remaining.length ? remaining : openWorkspace([], '/shipments'));
      if (activeWorkspace === id)
        location.hash = remaining[Math.max(0, index - 1)]?.route ?? '/shipments';
    });
  }
  const transportationId = /^\/transportations\/([0-9a-f-]{36})(?:\/route)?$/i.exec(route)?.[1];
  const shipmentId = /^\/shipments\/([0-9a-f-]{36})$/i.exec(route)?.[1];
  useEffect(() => {
    function changed() {
      if (!location.hash) {
        try {
          const pending = sessionStorage.getItem('broker:login-return');
          sessionStorage.removeItem('broker:login-return');
          if (
            pending &&
            /^\/(shipments|transportations|counterparties)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
              pending,
            )
          ) {
            location.replace('#' + pending);
            return;
          }
        } catch {
          /* При недоступном хранилище вход открывает реестр. */
        }
      }
      const next = location.hash.slice(1) || '/shipments';
      setRoute(next);
      if (next.startsWith('/transportations')) setTab('transportations');
      else if (next.startsWith('/counterparties')) setTab('counterparties');
      else setTab('shipments');
    }
    changed();
    window.addEventListener('hashchange', changed);
    return () => window.removeEventListener('hashchange', changed);
  }, []);
  useEffect(() => {
    const current = workspaces.find((entry) => entry.id === activeWorkspace);
    document.title = `${current?.title ?? 'Рабочее пространство'} · Broker`;
  }, [activeWorkspace, workspaces]);
  const [loadFailed, setLoadFailed] = useState(false);
  const [navigationPinned, setNavigationPinned] = useState(() => {
    try {
      return localStorage.getItem('broker:navigation-pinned') === 'true';
    } catch {
      return false;
    }
  });
  const [loading, setLoading] = useState(false);
  const [more, setMore] = useState(false);
  const [notice, setNotice] = useState('');
  const requestNumber = useRef(0);
  const updateShipment = useCallback((value: Shipment) => {
    setShipments((old) => old.map((item) => (item.id === value.id ? value : item)));
  }, []);
  const error = useCallback((value: unknown) => {
    if (value instanceof ApiError && value.status === 401) setSession(null);
    setNotice(
      value instanceof Error
        ? value.message +
            (value instanceof ApiError && value.traceId ? ` · Трейс: ${value.traceId}` : '')
        : 'Ошибка запроса',
    );
  }, []);
  useEffect(() => {
    const controller = new AbortController();
    request<Session>('/api/session', undefined, undefined, controller.signal)
      .then(setSession)
      .catch((value) => {
        if (!controller.signal.aborted) {
          setSession(null);
          if (!(value instanceof ApiError && value.status === 401)) error(value);
        }
      });
    return () => controller.abort();
  }, [error]);
  const load = useCallback(
    async (append = false) => {
      if (!session || tab === 'counterparties' || !session.permissions.includes(tab + '.read'))
        return;
      const currentRequest = ++requestNumber.current;
      setLoading(true);
      setLoadFailed(false);
      try {
        if (tab === 'shipments') {
          const page = await request<{ items: Shipment[] }>('/api/v1/shipments/list', session, {
            limit: 50,
            ...(append && shipments.length ? { after: shipments.at(-1)!.id } : {}),
          });
          if (currentRequest !== requestNumber.current) return;
          setShipments((old) => (append ? [...old, ...page.items] : page.items));
          setMore(page.items.length === 50);
        } else {
          const page = await request<{ items: Transportation[] }>(
            '/api/v1/transportations/list',
            session,
            {
              limit: 50,
              ...(append && transportations.length ? { after: transportations.at(-1)!.id } : {}),
            },
          );
          if (currentRequest !== requestNumber.current) return;
          setTransportations((old) => (append ? [...old, ...page.items] : page.items));
          setMore(page.items.length === 50);
        }
      } catch (value) {
        if (currentRequest === requestNumber.current) {
          setLoadFailed(true);
          error(value);
        }
      } finally {
        if (currentRequest === requestNumber.current) setLoading(false);
      }
    },
    [session, tab, shipments, transportations, error],
  );
  // Первую страницу запрашиваем только при входе или переключении раздела.
  useEffect(() => {
    if (session) void load();
    return () => {
      requestNumber.current++;
    };
  }, [session, tab]);
  useEffect(() => {
    if (!session?.permissions.includes('counterparties.read')) return;
    const controller = new AbortController();
    async function loadCustomers() {
      const items: Counterparty[] = [];
      const page = await request<{ items: Counterparty[] }>(
        '/api/v1/counterparties/list',
        session!,
        { status: 'all', limit: 100 },
        controller.signal,
      );
      items.push(...page.items);
      if (!controller.signal.aborted)
        setCustomers(items.sort((a, b) => a.name.localeCompare(b.name, 'ru')));
    }
    void loadCustomers().catch((value) => {
      if (!controller.signal.aborted) error(value);
    });
    return () => controller.abort();
  }, [session, error]);
  useEffect(() => {
    if (!session?.permissions.includes('counterparties.read')) return;
    const ids = [...new Set([...shipments.map((item) => item.customer_id)])].filter(
      (id) => !customers.some((item) => item.id === id),
    );
    if (!ids.length) return;
    const controller = new AbortController();
    void Promise.allSettled(
      ids.map((id) =>
        request<{ counterparty: Counterparty }>(
          '/api/v1/counterparties/get',
          session,
          { id },
          controller.signal,
        ),
      ),
    ).then((results) => {
      if (!controller.signal.aborted) {
        const found = results.flatMap((result) =>
          result.status === 'fulfilled' ? [result.value.counterparty] : [],
        );
        if (found.length)
          setCustomers((old) => [
            ...old,
            ...found.filter((item) => !old.some((value) => value.id === item.id)),
          ]);
      }
    });
    return () => controller.abort();
  }, [session, shipments, customers]);
  const customerName = (id: string) =>
    customers.find((item) => item.id === id)?.name || 'Клиент недоступен';
  const write = session?.permissions.includes('shipments.write') ?? false;
  async function logout() {
    try {
      const result = await request<{ logout_url: string }>('/api/logout', session!, {});
      location.assign(result.logout_url);
    } catch (value) {
      error(value);
    }
  }
  if (session === undefined)
    return (
      <div className="login">
        <Brand />
        <p>Открываем рабочее пространство…</p>
      </div>
    );
  if (!session)
    return (
      <div className="login">
        <Brand />
        <h1>Логистика начинается здесь</h1>
        <p>Поставки, позиции и перевозки в одном рабочем пространстве.</p>
        <a
          className="primary"
          href="/auth/login"
          onClick={() => {
            try {
              sessionStorage.setItem('broker:login-return', location.hash.slice(1));
            } catch {
              /* Сохранение адреса не блокирует вход. */
            }
          }}
        >
          Войти в Broker →
        </a>
        {notice && <p role="alert">{notice}</p>}
      </div>
    );
  return (
    <div className={`app${navigationPinned ? ' navigation-pinned' : ''}`}>
      <aside className="sidebar" id="workspace-navigation">
        <div className="sidebar-heading">
          <Brand />
          <button
            className="navigation-pin"
            aria-label={navigationPinned ? 'Открепить меню' : 'Закрепить меню'}
            aria-pressed={navigationPinned}
            title={navigationPinned ? 'Открепить меню' : 'Закрепить меню'}
            onClick={() => {
              const next = !navigationPinned;
              setNavigationPinned(next);
              try {
                localStorage.setItem('broker:navigation-pinned', String(next));
              } catch {
                /* Настройка доступна и без хранилища. */
              }
            }}
          >
            <svg
              width="18"
              height="18"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="1.7"
              strokeLinecap="round"
              strokeLinejoin="round"
              aria-hidden="true"
            >
              <path d="m16 3 5 5-4 1-3 5-4-4 5-3 1-4Z" />
              <path d="m10 10-3 3 4 4 3-3M9 15l-6 6" />
            </svg>
          </button>
        </div>
        <span className="nav-label">РАБОЧЕЕ ПРОСТРАНСТВО</span>
        <nav aria-label="Разделы">
          <button
            aria-label="Поставки"
            title="Поставки"
            aria-current={tab === 'shipments' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/shipments';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▣
            </span>{' '}
            <span className="navigation-text">Поставки</span>
          </button>
          <button
            aria-label="Перевозки"
            title="Перевозки"
            aria-current={tab === 'transportations' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/transportations';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ⇄
            </span>{' '}
            <span className="navigation-text">Перевозки</span>
          </button>
          <button
            aria-label="Контрагенты"
            title="Контрагенты"
            aria-current={tab === 'counterparties' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/counterparties';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▤
            </span>{' '}
            <span className="navigation-text">Контрагенты</span>
          </button>
        </nav>
        <div className="profile">
          <span className="avatar">
            {(session.display_name || session.subject).slice(0, 1).toUpperCase()}
          </span>
          <div>
            <strong title={session.display_name || session.subject}>
              {profileName(session.display_name || session.subject)}
            </strong>
            <button onClick={() => void logout()}>Выйти</button>
          </div>
        </div>
      </aside>
      <main>
        <header>
          <span>Операционный учёт</span>
          <span className="environment">Broker / Рабочее пространство</span>
        </header>
        <div className="workspace-tabs" role="tablist" aria-label="Открытые формы">
          {workspaces.map((entry) => (
            <div
              className={`workspace-tab${activeWorkspace === entry.id ? ' selected' : ''}`}
              key={entry.id}
            >
              <button
                type="button"
                role="tab"
                aria-selected={activeWorkspace === entry.id}
                onClick={() => {
                  location.hash = entry.route;
                }}
              >
                {entry.title}
              </button>
              <button
                type="button"
                className="workspace-close"
                aria-label={`Закрыть вкладку ${entry.title}`}
                onClick={() => closeWorkspace(entry.id)}
              >
                ×
              </button>
            </div>
          ))}
        </div>
        <div className="workspace-content">
          <div
            className={tab === 'transportations' ? 'transportation-registry' : undefined}
            hidden={
              tab === 'counterparties' ||
              creating ||
              Boolean(shipmentId) ||
              Boolean(transportationId) ||
              route.startsWith('/transportations/new/')
            }
          >
            <div className="page-heading">
              <div>
                {tab === 'shipments' && <p className="eyebrow">ЛОГИСТИКА</p>}
                <h1>{tab === 'shipments' ? 'Поставки' : 'Перевозки'}</h1>
                {tab === 'shipments' ? (
                  <p>От заявки клиента до фактических перевозок.</p>
                ) : (
                  <span className="registry-count" role="status">
                    {loading ? 'Загрузка…' : `Загружено: ${transportations.length}`}
                  </span>
                )}
              </div>
              <div className="actions">
                <button disabled={loading} onClick={() => void load()}>
                  Обновить
                </button>
                {tab === 'shipments' && write && (
                  <button
                    className="primary"
                    onClick={() => {
                      location.hash = '/shipments/new/' + crypto.randomUUID();
                    }}
                  >
                    ＋ Создать поставку
                  </button>
                )}
              </div>
            </div>
            <section className="surface" aria-busy={loading}>
              {tab === 'shipments' && (
                <div className="table-title">
                  <strong>Реестр поставок</strong>
                  <span>
                    {loading
                      ? 'Загрузка…'
                      : `Загружено: ${tab === 'shipments' ? shipments.length : transportations.length}`}
                  </span>
                </div>
              )}
              {tab === 'shipments' ? (
                <table>
                  <thead>
                    <tr>
                      <th>Поставка</th>
                      <th>Клиент</th>
                      <th>Примечание</th>
                      <th>Редакция</th>
                      <th />
                    </tr>
                  </thead>
                  <tbody>
                    {shipments.map((item) => (
                      <tr
                        key={item.id}
                        className="clickable"
                        onClick={() => {
                          location.hash = `/shipments/${item.id}`;
                        }}
                      >
                        <td>
                          <a
                            className="link"
                            href={`#/shipments/${item.id}`}
                            onClick={(event) => event.stopPropagation()}
                          >
                            {item.number}
                          </a>
                          <small>{item.id}</small>
                        </td>
                        <td>{customerName(item.customer_id)}</td>
                        <td>{item.reference || '—'}</td>
                        <td>
                          <span className="badge">{item.revision}</span>
                        </td>
                        <td>↗</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ) : (
                <TransportationTable items={transportations} />
              )}
              {!loading && !(tab === 'shipments' ? shipments.length : transportations.length) && (
                <div className="empty">
                  <span>▣</span>
                  <h2>{tab === 'shipments' ? 'Пока нет поставок' : 'Пока нет перевозок'}</h2>
                  <p>
                    {tab === 'shipments'
                      ? 'Создайте поставку, добавьте позиции и запустите создание перевозок.'
                      : 'Создайте перевозку в карточке поставки вручную или из плановой позиции.'}
                  </p>
                </div>
              )}
              <AutoLoadMore
                enabled={more}
                loading={loading}
                failed={loadFailed}
                onLoad={() => load(true)}
              />
            </section>
          </div>
          {workspaces
            .filter((entry) => entry.route !== '/shipments' && entry.route !== '/transportations')
            .map((entry) => (
              <div
                key={entry.id}
                hidden={activeWorkspace !== entry.id}
                data-workspace={entry.id}
                ref={(node) => {
                  if (node) workspaceNodes.current.set(entry.id, node);
                  else workspaceNodes.current.delete(entry.id);
                }}
              >
                {entry.route.startsWith('/counterparties') ? (
                  <CounterpartiesPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    session={session}
                    onError={error}
                    onChanged={(value) =>
                      setCustomers((old) => [...old.filter((item) => item.id !== value.id), value])
                    }
                    onSaved={(value) => {
                      const next = `/counterparties/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.name));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/shipments') ? (
                  <ShipmentWorkspace
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}
                    customers={customers}
                    onError={error}
                    onUpdate={updateShipment}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCreated={(value) => {
                      const next = `/shipments/${value.id}`;
                      setWorkspaces((old) =>
                        old.map((tab) =>
                          tab.id === entry.id
                            ? { id: next, route: next, title: value.number }
                            : tab,
                        ),
                      );
                      location.hash = next;
                      void load();
                    }}
                  />
                ) : entry.route.startsWith('/transportations/new/') ? (
                  <NewTransportation
                    shipmentId={entry.route.split('/')[3]}
                    session={session}
                    onError={error}
                    onSaved={(value) => {
                      const next = `/transportations/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.number));
                      location.hash = next;
                      void load();
                    }}
                  />
                ) : (
                  <TransportationPage
                    id={entry.id.split('/')[2]}
                    active={activeWorkspace === entry.id}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    editing={entry.route.endsWith('/route')}
                    session={session}
                    customers={customers}
                    onError={error}
                  />
                )}
              </div>
            ))}
        </div>
        {confirmation}
      </main>
      {notice && (
        <div className="toast" role="alert">
          <span>{notice}</span>
          <button aria-label="Закрыть уведомление" onClick={() => setNotice('')}>
            ×
          </button>
        </div>
      )}
    </div>
  );
}
export function Brand() {
  return (
    <div className="brand">
      <span>B</span>
      <span className="brand-name">
        broker<span className="brand-dot">.</span>
      </span>
    </div>
  );
}
