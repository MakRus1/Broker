import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import {
  ApiError,
  Commands,
  request,
  type Session,
  type Shipment,
  type Transportation,
} from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { ShipmentPanel } from './ShipmentPanel';
import { TransportationTable } from './components';

export function App() {
  const [session, setSession] = useState<Session | null>();
  const [tab, setTab] = useState<'shipments' | 'transportations'>('shipments');
  const [customers, setCustomers] = useState<Counterparty[]>([]);
  const [shipments, setShipments] = useState<Shipment[]>([]);
  const [transportations, setTransportations] = useState<Transportation[]>([]);
  const [selected, setSelected] = useState<Shipment | null>(null);
  const [route, setRoute] = useState(() => location.hash.slice(1) || '/shipments');
  const creating = route === '/shipments/new';
  const shipmentId = /^\/shipments\/([0-9a-f-]{36})$/i.exec(route)?.[1];
  useEffect(() => {
    function changed() {
      const next = location.hash.slice(1) || '/shipments';
      setRoute(next);
      if (next === '/transportations') setTab('transportations');
      else setTab('shipments');
    }
    changed();
    window.addEventListener('hashchange', changed);
    return () => window.removeEventListener('hashchange', changed);
  }, []);
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
      if (!session || !session.permissions.includes(tab + '.read')) return;
      const currentRequest = ++requestNumber.current;
      setLoading(true);
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
        if (currentRequest === requestNumber.current) error(value);
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
      while (!controller.signal.aborted) {
        const page = await request<{ items: Counterparty[] }>(
          '/api/v1/counterparties/list',
          session!,
          { limit: 100, ...(items.length ? { after: items.at(-1)!.id } : {}) },
          controller.signal,
        );
        items.push(...page.items);
        if (page.items.length < 100) break;
      }
      if (!controller.signal.aborted)
        setCustomers(items.sort((a, b) => a.name.localeCompare(b.name, 'ru')));
    }
    void loadCustomers().catch((value) => {
      if (!controller.signal.aborted) error(value);
    });
    return () => controller.abort();
  }, [session, error]);
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
        <a className="primary" href="/auth/login">
          Войти в Broker →
        </a>
        {notice && <p role="alert">{notice}</p>}
      </div>
    );
  return (
    <div className="app">
      <aside className="sidebar">
        <Brand />
        <span className="nav-label">РАБОЧЕЕ ПРОСТРАНСТВО</span>
        <nav aria-label="Разделы">
          <button
            aria-current={tab === 'shipments' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/shipments';
            }}
          >
            ▣ <span>Поставки</span>
          </button>
          <button
            aria-current={tab === 'transportations' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/transportations';
            }}
          >
            ⇄ <span>Перевозки</span>
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
        <div hidden={creating || Boolean(shipmentId)}>
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
            {more && (
              <div className="table-footer">
                <button disabled={loading} onClick={() => void load(true)}>
                  Загрузить ещё
                </button>
              </div>
            )}
          </section>
        </div>
        {shipmentId && !selected && <p className="page-heading">Открываем поставку…</p>}
        {shipmentId && selected && (
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
        {creating && (
          <section className="creation-page surface">
            <a href="#/shipments">← К списку поставок</a>
            <h1>Новая поставка</h1>
            <ShipmentForm
              key={session.actor}
              actor={session.actor}
              session={session}
              customers={customers}
              onCustomerCreated={(item) => setCustomers((old) => [...old, item])}
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
      <span>B</span>broker<span className="brand-dot">.</span>
    </div>
  );
}
function ShipmentForm({
  actor,
  session,
  customers,
  onCustomerCreated,
  onSubmit,
  onError,
}: {
  actor: string;
  session: Session;
  customers: Counterparty[];
  onCustomerCreated: (item: Counterparty) => void;
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
        onCreated={onCustomerCreated}
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
