import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  useSyncExternalStore,
  type ReactNode,
} from 'react';
import { createPortal } from 'react-dom';
import { usePreferencesSession } from '../preferences/UserPreferences';
import { useLocalization } from '../localization/Localization';
import { NotificationStore, type NotificationItem } from './notificationStore';
import './notifications.css';
import { request, ApiError } from '../../shared/api/http';
const Context = createContext<NotificationStore | undefined>(undefined);
export function useNotifications() {
  const store = useContext(Context);
  if (!store) throw new Error('NotificationProvider is missing');
  return store;
}
export function NotificationProvider({ children }: { children: ReactNode }) {
  const [session] = usePreferencesSession();
  const store = useMemo(
    () => new NotificationStore(),
    [session?.actor, session?.subject, session?.organization_id],
  );
  return (
    <Context.Provider value={store}>
      {session && <ServerNotifications />}
      {children}
      {session && <NotificationToasts />}
    </Context.Provider>
  );
}
interface ServerNotification {
  event_type: 'shipment.created' | 'shipment.route_changed';
  id: string;
  shipment_id: string;
  shipment_number: string;
  shipment_revision: number;
  created_at: string;
  read: boolean;
}
function ServerNotifications() {
  const [session] = usePreferencesSession();
  const store = useNotifications();
  const { translate } = useLocalization();
  useEffect(() => {
    if (!session || !session.permissions.includes('shipments.read')) return;
    const controller = new AbortController();
    let timer: ReturnType<typeof setTimeout> | undefined;
    let running = false;
    let failures = 0;
    store.setRemoteRead(async (ids, dismiss = false) => {
      await request(
        '/api/notifications/read',
        session,
        {
          organization_id: session.organization_id,
          ids: ids.map((id) => id.slice(7)),
          dismiss,
        },
        controller.signal,
      );
    });
    const refresh = async () => {
      if (running || controller.signal.aborted) return;
      if (timer) clearTimeout(timer);
      if (document.visibilityState === 'hidden') return;
      running = true;
      try {
        const result = await request<{ items: ServerNotification[] }>(
          '/api/notifications/list',
          session,
          { organization_id: session.organization_id, limit: 100 },
          controller.signal,
        );
        if (controller.signal.aborted) return;
        store.mergeRemote(
          result.items.map((item) => ({
            id: `server:${item.id}`,
            remote: true,
            message: translate(
              item.event_type === 'shipment.created'
                ? 'notifications.shipmentCreated'
                : 'notifications.shipmentRouteChanged',
              {
                number: item.shipment_number,
              },
            ),
            level: 'info',
            route: `/shipments/${item.shipment_id}${item.event_type === 'shipment.created' ? '' : '/route'}`,
            createdAt: Date.parse(item.created_at),
            read: item.read,
            toast: false,
            resolved: false,
          })),
        );
        failures = 0;
      } catch (error) {
        if (error instanceof ApiError && (error.status === 401 || error.status === 403))
          store.clearRemote();
        failures = Math.min(failures + 1, 4);
      } finally {
        running = false;
        if (!controller.signal.aborted)
          timer = setTimeout(
            () => void refresh(),
            Math.min(20000 * 2 ** failures, 120000) + Math.random() * 2000,
          );
      }
    };
    const visible = () => {
      if (document.visibilityState === 'visible') void refresh();
    };
    void refresh();
    document.addEventListener('visibilitychange', visible);
    return () => {
      controller.abort();
      if (timer) clearTimeout(timer);
      document.removeEventListener('visibilitychange', visible);
    };
  }, [session, store, translate]);
  return null;
}
function Bell() {
  return (
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
      <path d="M18 8a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9M10 21h4" />
    </svg>
  );
}
function CloseIcon() {
  return (
    <svg
      width="16"
      height="16"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      aria-hidden="true"
    >
      <path d="m6 6 12 12M18 6 6 18" />
    </svg>
  );
}
function NotificationCard({
  item,
  toast = false,
  onNavigate,
}: {
  item: NotificationItem;
  toast?: boolean;
  onNavigate?: () => void;
}) {
  const store = useNotifications();
  const { translate, locale } = useLocalization();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    if (!toast || (item.level !== 'success' && item.level !== 'info')) return;
    const timer = setTimeout(() => store.hideToast(item.id), item.level === 'info' ? 7000 : 4500);
    return () => clearTimeout(timer);
  }, [toast, item.id, item.createdAt, item.level, store]);
  const level = item.resolved
    ? translate('notifications.resolved')
    : translate(`notifications.${item.level}`);
  return (
    <article
      className="notification-card"
      data-level={item.resolved ? 'resolved' : item.level}
      data-unread={!item.read}
    >
      <div className="notification-card-heading">
        <span>{level}</span>
        <time dateTime={new Date(item.createdAt).toISOString()}>
          {new Intl.DateTimeFormat(locale, { hour: '2-digit', minute: '2-digit' }).format(
            item.createdAt,
          )}
        </time>
        {toast && (
          <button
            className="notification-icon"
            aria-label={translate('notifications.dismiss')}
            onClick={() => store.hideToast(item.id)}
          >
            <CloseIcon />
          </button>
        )}
      </div>
      <p>{item.message}</p>
      {failure && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      <div className="notification-actions">
        {item.route && (
          <button
            onClick={() => {
              store.markRead(item.id);
              location.hash = item.route!;
              onNavigate?.();
            }}
          >
            {translate('notifications.open')}
          </button>
        )}
        {item.action && !item.resolved && (
          <button
            disabled={busy}
            onClick={async () => {
              setBusy(true);
              setFailure('');
              try {
                await item.action!.run();
              } catch (error) {
                setFailure(error instanceof Error ? error.message : translate('errors.request'));
              } finally {
                setBusy(false);
              }
            }}
          >
            {item.action.label}
          </button>
        )}
        {!toast && !item.read && (
          <button onClick={() => store.markRead(item.id)}>
            {translate('notifications.markRead')}
          </button>
        )}
      </div>
    </article>
  );
}
function NotificationToasts() {
  const store = useNotifications();
  const items = useSyncExternalStore(store.subscribe, store.snapshot);
  const { translate } = useLocalization();
  return createPortal(
    <div
      className="notification-toasts"
      role="region"
      aria-label={translate('notifications.title')}
      aria-live="polite"
      aria-relevant="additions text"
    >
      {items
        .filter((item) => item.toast)
        .slice(0, 3)
        .map((item) => (
          <NotificationCard key={item.id} item={item} toast />
        ))}
    </div>,
    document.body,
  );
}
export function NotificationCenter() {
  const store = useNotifications();
  const items = useSyncExternalStore(store.subscribe, store.snapshot);
  const { translate } = useLocalization();
  const [open, setOpen] = useState(false);
  const [unreadOnly, setUnreadOnly] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const panel = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
  const unread = items.filter((item) => !item.read).length;
  useEffect(() => {
    if (!open) return;
    panel.current?.querySelector<HTMLButtonElement>('button')?.focus();
    const dismiss = (event: PointerEvent) => {
      if (
        event.target instanceof Node &&
        !root.current?.contains(event.target) &&
        !panel.current?.contains(event.target)
      )
        setOpen(false);
    };
    const escape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        event.preventDefault();
        setOpen(false);
        trigger.current?.focus();
      }
    };
    document.addEventListener('pointerdown', dismiss);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', dismiss);
      document.removeEventListener('keydown', escape);
    };
  }, [open]);
  return (
    <div className="notification-center" ref={root}>
      <button
        ref={trigger}
        className="notification-trigger"
        aria-label={translate('notifications.count', { count: unread })}
        title={translate('notifications.title')}
        aria-expanded={open}
        onClick={() => setOpen(!open)}
      >
        <Bell />
        {unread > 0 && (
          <span className="notification-badge" aria-hidden="true">
            {unread > 99 ? '99+' : unread}
          </span>
        )}
      </button>
      {open &&
        createPortal(
          <div
            ref={panel}
            className="notification-panel"
            role="region"
            aria-label={translate('notifications.title')}
          >
            <div className="notification-panel-heading">
              <h2>{translate('notifications.title')}</h2>
              <button
                className="notification-icon"
                aria-label={translate('ui.close')}
                onClick={() => {
                  setOpen(false);
                  trigger.current?.focus();
                }}
              >
                <CloseIcon />
              </button>
            </div>
            <div className="notification-panel-tools">
              <button aria-pressed={unreadOnly} onClick={() => setUnreadOnly(!unreadOnly)}>
                {translate('notifications.unread')}
              </button>
              <button disabled={!unread} onClick={() => store.markRead()}>
                {translate('notifications.readAll')}
              </button>
              <button disabled={!items.some((item) => item.read)} onClick={store.clearRead}>
                {translate('notifications.clearRead')}
              </button>
            </div>
            <div className="notification-list">
              {items
                .filter((item) => !unreadOnly || !item.read)
                .map((item) => (
                  <NotificationCard key={item.id} item={item} onNavigate={() => setOpen(false)} />
                ))}
              {!items.some((item) => !unreadOnly || !item.read) && (
                <p className="notification-empty">{translate('notifications.empty')}</p>
              )}
            </div>
            <p className="notification-session-hint">{translate('notifications.session')}</p>
          </div>,
          document.body,
        )}
    </div>
  );
}
