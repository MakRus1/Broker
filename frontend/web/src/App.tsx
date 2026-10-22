import { AutoLoadMore } from './AutoLoadMore';
import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import {
  ApiError,
  Commands,
  request,
  type Session,
  type Shipment,
  type Transportation,
} from './api';
import { CounterpartiesPage } from './CounterpartiesPage';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { TransportationPage } from './TransportationPage';
import { ShipmentPanel } from './ShipmentPanel';
import { TransportationTable } from './components';

export function App() {
  const [session, setSession] = useState<Session | null>();
  const [tab, setTab] = useState<'shipments' | 'transportations' | 'counterparties'>('shipments');
  const [customers, setCustomers] = useState<Counterparty[]>([]);
  const [shipments, setShipments] = useState<Shipment[]>([]);
  const [transportations, setTransportations] = useState<Transportation[]>([]);
  const [selected, setSelected] = useState<Shipment | null>(null);
  const [route, setRoute] = useState(() => location.hash.slice(1) || '/shipments');
  const creating = route === '/shipments/new';
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
            /^\/(shipments|transportations|counterparties)(?:\/(?:new|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
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
    document.title = creating
      ? 'Новая поставка · Broker'
      : shipmentId || transportationId
        ? 'Broker'
        : `${tab === 'shipments' ? 'Поставки' : tab === 'counterparties' ? 'Контрагенты' : 'Перевозки'} · Broker`;
  }, [creating, shipmentId, transportationId, tab]);
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
  const commands = useRef(new Commands());
  const requestNumber = useRef(0);
  const closeSelected = useCallback(() => {
    location.hash = '/shipments';
  }, []);
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
    setSelected(null);
    if (!session || !shipmentId) return;
    const controller = new AbortController();
    request<{ shipment: Shipment }>(
      '/api/v1/shipments/get',
      session,
      { shipment_id: shipmentId },
      controller.signal,
    )
      .then(({ shipment }) => setSelected(shipment))
      .catch((value) => {
        if (!controller.signal.aborted) error(value);
      });
    return () => controller.abort();
  }, [session, shipmentId, error]);
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
    const ids = [
      ...new Set([
        ...shipments.map((item) => item.customer_id),
        ...(selected ? [selected.customer_id] : []),
      ]),
    ].filter((id) => !customers.some((item) => item.id === id));
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
  }, [session, shipments, selected, customers]);
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
            <strong>{session.display_name || session.subject}</strong>
            <button onClick={() => void logout()}>Выйти</button>
          </div>
        </div>
      </aside>
      <main>
        <header>
          <span>Операционный учёт</span>
          <span className="environment">Broker / Рабочее пространство</span>
        </header>
        <div
          hidden={
            tab === 'counterparties' || creating || Boolean(shipmentId) || Boolean(transportationId)
          }
        >
          <div className="page-heading">
            <div>
              <p className="eyebrow">ЛОГИСТИКА</p>
              <h1>{tab === 'shipments' ? 'Поставки' : 'Перевозки'}</h1>
              <p>
                {tab === 'shipments'
                  ? 'От заявки клиента до фактических перевозок.'
                  : 'Фактические перевозки и их маршруты.'}
              </p>
            </div>
            <div className="actions">
              <button disabled={loading} onClick={() => void load()}>
                Обновить
              </button>
              {tab === 'shipments' && write && (
                <button
                  className="primary"
                  onClick={() => {
                    location.hash = '/shipments/new';
                  }}
                >
                  ＋ Создать поставку
                </button>
              )}
            </div>
          </div>
          <section className="surface" aria-busy={loading}>
            <div className="table-title">
              <strong>{tab === 'shipments' ? 'Реестр поставок' : 'Реестр перевозок'}</strong>
              <span>
                {loading
                  ? 'Загрузка…'
                  : `Загружено: ${tab === 'shipments' ? shipments.length : transportations.length}`}
              </span>
            </div>
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
                    : 'Перевозки появятся после создания из позиций поставки.'}
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
        {tab === 'counterparties' && (
          <CounterpartiesPage
            route={route}
            session={session}
            onError={error}
            onChanged={(value) =>
              setCustomers((old) => [...old.filter((item) => item.id !== value.id), value])
            }
          />
        )}
        {shipmentId && selected?.id !== shipmentId && (
          <p className="page-heading">Открываем поставку…</p>
        )}
        {shipmentId && selected?.id === shipmentId && (
          <ShipmentPanel
            key={selected.id}
            customerName={customerName(selected.customer_id)}
            initial={selected}
            session={session}
            onClose={closeSelected}
            onUpdate={updateShipment}
            onError={error}
          />
        )}
        {transportationId && (
          <TransportationPage
            key={transportationId}
            id={transportationId}
            editing={route.endsWith('/route')}
            session={session}
            customers={customers}
            onError={error}
          />
        )}
        {creating && (
          <section className="creation-page surface">
            <a href="#/shipments">← К списку поставок</a>
            <h1>Новая поставка</h1>
            <ShipmentForm
              key={session.actor}
              actor={session.actor}
              session={session}
              customers={customers}
              onSubmit={async (body) => {
                const result = await commands.current.send<{ shipment: Shipment }>(
                  '/api/v1/shipments/create',
                  session,
                  body,
                );
                try {
                  sessionStorage.removeItem(`broker:shipment-draft:${session.actor}`);
                } catch {
                  /* Сохранение поставки не зависит от доступности хранилища браузера. */
                }
                location.hash = `/shipments/${result.shipment.id}`;
                void load();
              }}
              onError={error}
            />
          </section>
        )}
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
function ShipmentForm({
  actor,
  session,
  customers,
  onSubmit,
  onError,
}: {
  actor: string;
  session: Session;
  customers: Counterparty[];
  onSubmit: (body: object) => Promise<void>;
  onError: (value: unknown) => void;
}) {
  const draftKey = `broker:shipment-draft:${actor}`;
  const [draft] = useState(() => {
    try {
      const value: unknown = JSON.parse(sessionStorage.getItem(draftKey) || '{}');
      if (!value || typeof value !== 'object') return {};
      const fields = value as Record<string, unknown>;
      return {
        customer: typeof fields.customer === 'string' ? fields.customer : '',
        reference: typeof fields.reference === 'string' ? fields.reference : '',
      };
    } catch {
      return {};
    }
  });
  const [customer, setCustomer] = useState<string>(draft.customer || '');
  const [reference, setReference] = useState<string>(draft.reference || '');
  useEffect(() => {
    try {
      sessionStorage.setItem(draftKey, JSON.stringify({ customer, reference }));
    } catch {
      /* При запрете хранилища поля сохраняются до ухода со страницы. */
    }
  }, [draftKey, customer, reference]);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    function leaving(event: BeforeUnloadEvent) {
      if (!customer && !reference && !busy) return;
      event.preventDefault();
      event.returnValue = '';
    }
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [customer, reference, busy]);
  async function submit(event: FormEvent) {
    event.preventDefault();
    setBusy(true);
    setFailure('');
    try {
      await onSubmit({ customer_id: customer.trim(), reference: reference.trim() });
    } catch (value) {
      setFailure(value instanceof Error ? value.message : 'Ошибка сохранения');
      onError(value);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form onSubmit={submit} data-dirty={Boolean(customer || reference)} data-busy={busy}>
      <CustomerSelect
        items={customers}
        value={customer}
        onChange={setCustomer}
        session={session}
        disabled={busy}
      />
      <label>
        Примечание
        <textarea
          disabled={busy}
          maxLength={256}
          value={reference}
          onChange={(event) => setReference(event.target.value)}
          placeholder="Что важно знать об этой поставке"
        />
      </label>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <button disabled={busy} className="primary">
        {busy ? 'Создаём…' : 'Создать поставку'}
      </button>
    </form>
  );
}
