import { PaymentsPage } from './PaymentsPage';
import { InvoicesPage } from './InvoicesPage';
import { useRouteRegistryFilters } from './RouteRegistryFilters';
import { preferenceKey, useRegistryPreference } from './registryPreferences';
import { restoreShipmentQuery, restoreTransportationQuery } from './registryQueryPreferences';
import { useColumnFilters } from './RegistryColumnFilters';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from './RegistryTable';
import { transportationColumns } from './components';
import {
  initialShipmentQuery,
  shipmentRegistryRequest,
  type ShipmentRegistryCursor,
  type ShipmentSort,
} from './shipmentRegistry';
import { CalendarDate } from './CalendarDate';
import { transportationTitle } from './transportation';
import {
  registryRequest,
  initialRegistryQuery,
  type RegistryCursor,
  type RegistrySort,
} from './transportationRegistry';
import {
  openWorkspace,
  replaceWorkspace,
  workspaceId,
  moveWorkspace,
  restoreWorkspaces,
  type WorkspaceTab,
} from './workspace';
import { WorkspaceTabs } from './WorkspaceTabs';
import { ShipmentWorkspace } from './ShipmentWorkspace';
import { useDiscardConfirmation } from './components';
import { profileName } from './profile';
import { AutoLoadMore } from './AutoLoadMore';
import { useCallback, useEffect, useRef, useState } from 'react';
import { ApiError, request, type Session, type Shipment, type Transportation } from './api';
import { FinancialLabelsPage } from './FinancialLabelsPage';
import { ContractsPage } from './ContractsPage';
import { CounterpartiesPage } from './CounterpartiesPage';
import { type Counterparty } from './CustomerSelect';
import { TransportationPage } from './TransportationPage';
import { NewTransportation } from './TransportationDetailsForm';
import { TransportationTable } from './components';

export function App() {
  const [session, setSession] = useState<Session | null>();
  const [tab, setTab] = useState<
    | 'shipments'
    | 'transportations'
    | 'counterparties'
    | 'contracts'
    | 'financial-labels'
    | 'incoming-invoices'
    | 'outgoing-invoices'
    | 'receipts'
  >('shipments');
  const [customers, setCustomers] = useState<Counterparty[]>([]);
  const [shipments, setShipments] = useState<Shipment[]>([]);
  const [transportations, setTransportations] = useState<Transportation[]>([]);

  const shipmentColumns: RegistryColumn<Shipment>[] = [
    {
      id: 'number',
      label: 'Поставка',
      width: 180,
      required: true,
      sort: 'number',
      render: (item) => (
        <a
          className="link"
          href={`#/shipments/${item.id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {item.number}
        </a>
      ),
    },
    {
      id: 'shipment_date',
      label: 'Дата поставки',
      width: 190,
      sort: 'shipment_date',
      render: (item) =>
        item.shipment_date ? CalendarDate.fromISO(item.shipment_date).toDisplayString() : '—',
    },
    {
      id: 'customer',
      filterFields: ['customer_id'],
      label: 'Клиент',
      width: 280,
      render: (item) => customerName(item.customer_id),
    },
    {
      id: 'cargo_description',
      label: 'Груз',
      width: 340,
      sort: 'cargo_description',
      render: (item) => item.cargo_description || '—',
    },
    {
      id: 'status',
      label: 'Статус',
      width: 180,
      sort: 'status',
      render: (item) => <span className="badge">{item.status || '—'}</span>,
    },
  ];
  const shipmentColumnState = useRegistryColumns(
    session?.subject ?? '',
    'shipments',
    shipmentColumns,
  );
  const transportationColumnState = useRegistryColumns(
    session?.subject ?? '',
    'transportations',
    transportationColumns,
  );
  const [route, setRoute] = useState(() => location.hash.slice(1) || '/shipments');
  const creating = /^\/shipments\/new(?:\/|$)/.test(route);
  const [workspaces, setWorkspaces] = useState<WorkspaceTab[]>(() => openWorkspace([], route));
  const activeWorkspace = workspaceId(route);
  const [restoredFor, setRestoredFor] = useState('');
  const storageKey = session ? `broker:workspaces:${session.subject}` : '';
  useEffect(() => {
    if (!storageKey) return;
    let saved: string | null = null;
    try {
      saved = sessionStorage.getItem(storageKey);
    } catch {
      /* Вкладки работают без хранилища. */
    }
    setWorkspaces(restoreWorkspaces(saved, location.hash.slice(1) || '/shipments'));
    setRestoredFor(storageKey);
  }, [storageKey]);
  useEffect(() => {
    if (!storageKey || restoredFor !== storageKey) return;
    try {
      sessionStorage.setItem(storageKey, JSON.stringify(workspaces));
    } catch {
      /* Недоступность хранилища не мешает менять порядок вкладок. */
    }
  }, [storageKey, restoredFor, workspaces]);
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
  const shipmentId = /^\/shipments\/([0-9a-f-]{36})(?:\/route)?$/i.exec(route)?.[1];
  useEffect(() => {
    function changed() {
      if (!location.hash) {
        try {
          const pending = sessionStorage.getItem('broker:login-return');
          sessionStorage.removeItem('broker:login-return');
          if (
            pending &&
            /^\/(shipments|transportations|counterparties|contracts|financial-labels|incoming-invoices|outgoing-invoices|receipts)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
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
      else if (next.startsWith('/financial-labels')) setTab('financial-labels');
      else if (next.startsWith('/incoming-invoices')) setTab('incoming-invoices');
      else if (next.startsWith('/receipts')) setTab('receipts');
      else if (next.startsWith('/outgoing-invoices')) setTab('outgoing-invoices');
      else if (next.startsWith('/contracts')) setTab('contracts');
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
  const [shipmentQuery, setShipmentQuery] = useRegistryPreference(
    preferenceKey(session?.subject ?? '', 'shipments', 'query'),
    initialShipmentQuery,
    restoreShipmentQuery,
  );
  const [shipmentCursor, setShipmentCursor] = useState<ShipmentRegistryCursor>();
  const [registryQuery, setRegistryQuery] = useRegistryPreference(
    preferenceKey(session?.subject ?? '', 'transportations', 'query'),
    initialRegistryQuery,
    restoreTransportationQuery,
  );
  const shipmentFilters = useColumnFilters({
    registry: 'shipments',
    session,
    query: shipmentQuery,
    onChange: (filters) => {
      setShipmentCursor(undefined);
      setShipmentQuery((old) => ({ ...old, filters }));
    },
    onSort: (sort, descending) => {
      setShipmentCursor(undefined);
      setShipmentQuery((old) => ({ ...old, sort: sort as ShipmentSort, descending }));
    },
  });
  const routeFilters = useRouteRegistryFilters(session, registryQuery, (route_filters) => {
    setRegistryCursor(undefined);
    setRegistryQuery((old) => ({ ...old, route_filters }));
  });
  const transportationFilters = useColumnFilters({
    registry: 'transportations',
    session,
    query: registryQuery,
    onChange: (filters) => {
      setRegistryCursor(undefined);
      setRegistryQuery((old) => ({ ...old, filters }));
    },
    onSort: (sort, descending) => {
      setRegistryCursor(undefined);
      setRegistryQuery((old) => ({ ...old, sort: sort as RegistrySort, descending }));
    },
  });
  const [registryCursor, setRegistryCursor] = useState<RegistryCursor>();
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
      if (
        !session ||
        tab === 'receipts' ||
        tab === 'counterparties' ||
        tab === 'incoming-invoices' ||
        tab === 'outgoing-invoices' ||
        tab === 'contracts' ||
        tab === 'financial-labels' ||
        !session.permissions.includes(tab + '.read')
      )
        return;
      const currentRequest = ++requestNumber.current;
      if (!append) {
        if (tab === 'transportations') setTransportations([]);
        else setShipments([]);
        setMore(false);
      }
      setLoading(true);
      setLoadFailed(false);
      try {
        if (tab === 'shipments') {
          const page = await request<{ items: Shipment[]; next_cursor?: ShipmentRegistryCursor }>(
            '/api/v1/shipments/list',
            session,
            shipmentRegistryRequest(shipmentQuery, append ? shipmentCursor : undefined),
          );
          if (currentRequest !== requestNumber.current) return;
          setShipments((old) => (append ? [...old, ...page.items] : page.items));
          setShipmentCursor(page.next_cursor);
          setMore(Boolean(page.next_cursor));
        } else {
          const page = await request<{ items: Transportation[]; next_cursor?: RegistryCursor }>(
            '/api/v1/transportations/list',
            session,
            registryRequest(registryQuery, append ? registryCursor : undefined),
          );
          if (currentRequest !== requestNumber.current) return;
          setTransportations((old) => (append ? [...old, ...page.items] : page.items));
          setRegistryCursor(page.next_cursor);
          setMore(Boolean(page.next_cursor));
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
    [
      session,
      tab,
      shipments,
      transportations,
      registryQuery,
      registryCursor,
      shipmentQuery,
      shipmentCursor,
      error,
    ],
  );
  // Первую страницу запрашиваем только при входе или переключении раздела.
  useEffect(() => {
    if (session) void load();
    return () => {
      requestNumber.current++;
    };
  }, [session, tab, registryQuery, shipmentQuery]);
  useEffect(() => {
    if (!session?.permissions.includes('counterparties.read')) return;
    const controller = new AbortController();
    async function loadCustomers() {
      const items: Counterparty[] = [];
      const page = await request<{ items: Counterparty[] }>(
        '/api/v1/counterparties/list',
        session!,
        { limit: 100 },
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
    const ids = [...new Set(shipments.map((item) => item.customer_id))]
      .filter((id) => !customers.some((item) => item.id === id))
      .slice(0, 100);
    if (!ids.length) return;
    const controller = new AbortController();
    void request<{ items: Counterparty[] }>(
      '/api/v1/counterparties/list',
      session,
      { filters: [{ field: 'id', values: ids }], limit: 100 },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted && result.items.length)
          setCustomers((old) => [
            ...old,
            ...result.items.filter((item) => !old.some((known) => known.id === item.id)),
          ]);
      })
      .catch((reason: unknown) => {
        if (!controller.signal.aborted) error(reason);
      });
    return () => controller.abort();
  }, [session, shipments, customers, error]);
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
          <button
            aria-label="Договоры"
            title="Договоры"
            aria-current={tab === 'contracts' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/contracts';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▧
            </span>{' '}
            <span className="navigation-text">Договоры</span>
          </button>
          {session.permissions.includes('finance.read_payments') && (
            <button
              aria-label="Поступления"
              title="Поступления"
              aria-current={tab === 'receipts' ? 'page' : undefined}
              onClick={() => {
                location.hash = '/receipts';
              }}
            >
              <span className="navigation-icon" aria-hidden="true">
                ↓
              </span>
              <span className="navigation-text">Поступления</span>
            </button>
          )}
          {session.permissions.includes('finance.read_invoices') &&
            (['incoming', 'outgoing'] as const).map((direction) => (
              <button
                key={direction}
                aria-label={direction === 'incoming' ? 'Входящие счета' : 'Исходящие счета'}
                title={direction === 'incoming' ? 'Входящие счета' : 'Исходящие счета'}
                aria-current={tab === `${direction}-invoices` ? 'page' : undefined}
                onClick={() => {
                  location.hash = `/${direction}-invoices`;
                }}
              >
                <span className="navigation-icon" aria-hidden="true">
                  {direction === 'incoming' ? '↙' : '↗'}
                </span>
                <span className="navigation-text">
                  {direction === 'incoming' ? 'Входящие счета' : 'Исходящие счета'}
                </span>
              </button>
            ))}
          <button
            aria-label="Финансовые метки"
            title="Финансовые метки"
            aria-current={tab === 'financial-labels' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/financial-labels';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ₽
            </span>{' '}
            <span className="navigation-text">Финансовые метки</span>
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
        <WorkspaceTabs
          tabs={workspaces}
          active={activeWorkspace}
          onMove={(source, target, side) =>
            setWorkspaces((old) => moveWorkspace(old, source, target, side))
          }
          onClose={closeWorkspace}
        />
        <div className="workspace-content">
          <div
            className={tab === 'transportations' ? 'transportation-registry' : 'shipment-registry'}
            hidden={
              tab === 'receipts' ||
              tab === 'counterparties' ||
              tab === 'incoming-invoices' ||
              tab === 'outgoing-invoices' ||
              tab === 'contracts' ||
              tab === 'financial-labels' ||
              creating ||
              Boolean(shipmentId) ||
              Boolean(transportationId) ||
              route.startsWith('/transportations/new/')
            }
          >
            <div className="page-heading">
              <div>
                <h1>{tab === 'shipments' ? 'Поставки' : 'Перевозки'}</h1>
                <span className="registry-count" role="status">
                  {loading
                    ? 'Загрузка…'
                    : `Загружено: ${tab === 'shipments' ? shipments.length : transportations.length}`}
                </span>
              </div>
              <div className="actions">
                <RegistryColumnSettings
                  {...(tab === 'shipments' ? shipmentColumnState : transportationColumnState)}
                />
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
              {tab === 'shipments' ? (
                <RegistryTable
                  items={shipments}
                  columns={shipmentColumnState}
                  definitions={shipmentColumns}
                  filters={shipmentFilters}
                  sort={shipmentQuery.sort}
                  descending={shipmentQuery.descending}
                  onSort={(field) => {
                    const sort = field as ShipmentSort;
                    setShipmentCursor(undefined);
                    setShipmentQuery((old) => ({
                      ...old,
                      sort,
                      descending: old.sort === sort ? !old.descending : false,
                    }));
                  }}
                  onOpen={(item) => {
                    location.hash = `/shipments/${item.id}`;
                  }}
                />
              ) : (
                <>
                  {routeFilters.editor}
                  <TransportationTable
                    routeFilter={routeFilters.button}
                    routeFilterSummary={routeFilters.toolbar}
                    items={transportations}
                    filters={transportationFilters}
                    columns={transportationColumnState}
                    sort={registryQuery.sort}
                    descending={registryQuery.descending}
                    onSort={(sort: RegistrySort) => {
                      setRegistryCursor(undefined);
                      setRegistryQuery((old) => ({
                        ...old,
                        sort,
                        descending: old.sort === sort ? !old.descending : false,
                      }));
                    }}
                  />
                </>
              )}
              {!loading && !(tab === 'shipments' ? shipments.length : transportations.length) && (
                <div className="empty">
                  <span>▣</span>
                  <h2>{tab === 'shipments' ? 'Поставки не найдены' : 'Перевозки не найдены'}</h2>
                  <p>
                    {tab === 'shipments'
                      ? 'Измените фильтры или создайте поставку.'
                      : 'Измените фильтры или создайте перевозку в карточке поставки.'}
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
                {entry.route.startsWith('/receipts') ? (
                  <PaymentsPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancelNew={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = '/receipts';
                    }}
                    onSaved={(value) => {
                      const next = `/receipts/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.number));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/incoming-invoices') ||
                  entry.route.startsWith('/outgoing-invoices') ? (
                  <InvoicesPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}

                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancelNew={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = entry.route.split('/').slice(0, 2).join('/');
                    }}
                    onSaved={(value) => {
                      const next = `/${value.direction}-invoices/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.number));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/financial-labels') ? (
                  <FinancialLabelsPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}
                    onError={error}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancelNew={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = '/financial-labels';
                    }}
                    onSaved={(value) => {
                      const next = `/financial-labels/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.name));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/contracts') ? (
                  <ContractsPage
                    active={activeWorkspace === entry.id}
                    route={entry.route}
                    session={session}
                    onError={error}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancelNew={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = '/contracts';
                    }}
                    onSaved={(value) => {
                      const next = `/contracts/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.number));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/counterparties') ? (
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
                      setWorkspaces((old) =>
                        replaceWorkspace(old, entry.id, next, transportationTitle(value)),
                      );
                      location.hash = next;
                      void load();
                    }}
                  />
                ) : (
                  <TransportationPage
                    onChanged={(value) =>
                      setTransportations((old) =>
                        old.map((item) => (item.id === value.id ? value : item)),
                      )
                    }
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
