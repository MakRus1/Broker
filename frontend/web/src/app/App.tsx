import { ShipmentRegistry } from '../features/shipments/ShipmentRegistryPage';
import { TransportationRegistry } from '../features/transportations/TransportationRegistryPage';
import { WorkspaceHome } from './WorkspaceHome';
import { ServiceChargePage } from '../features/finance/services/ServiceChargePage';
import { ServiceCharges } from '../features/finance/FinancePanel';
import { PaymentsPage } from '../features/finance/payments/PaymentsPage';
import { InvoicesPage } from '../features/finance/invoices/InvoicesPage';
import { transportationTitle } from '../features/transportations/transportation';
import {
  openWorkspace,
  replaceWorkspace,
  workspaceId,
  localizedWorkspaceTitle,
  moveWorkspace,
  restoreWorkspaces,
  initialWorkspaceRoute,
  type WorkspaceTab,
} from './workspace';
import { WorkspaceTabs } from './WorkspaceTabs';
import { ShipmentWorkspace } from '../features/shipments/ShipmentWorkspace';
import { useDiscardConfirmation } from '../shared/ui/Dialog';
import { profileName } from '../shared/profile/profile';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { ApiError, request } from '../shared/api/http';
import { FinancialLabelsPage } from '../features/finance/labels/FinancialLabelsPage';
import { ContractsPage } from '../features/contracts/ContractsPage';
import { CounterpartiesPage } from '../features/counterparties/CounterpartiesPage';
import { type Counterparty } from '../features/counterparties/CustomerSelect';
import { TransportationPage } from '../features/transportations/TransportationPage';
import { NewTransportation } from '../features/transportations/TransportationDetailsForm';

import { usePreferencesSession } from '../features/preferences/UserPreferences';

import { initialThemePreference } from '../../../shared/theme/theme';
import { ThemeControl } from '../features/appearance/ThemeControl';

import { useLocalization } from '../features/localization/Localization';
import { LanguageControl } from '../features/localization/LanguageControl';

export function App() {
  const { translate, locale } = useLocalization();
  const [session, setSession] = usePreferencesSession();
  const [tab, setTab] = useState<
    | 'home'
    | 'shipments'
    | 'transportations'
    | 'counterparties'
    | 'contracts'
    | 'service-charges'
    | 'financial-labels'
    | 'incoming-invoices'
    | 'outgoing-invoices'
    | 'receipts'
    | 'disbursements'
  >('home');
  const [customers, setCustomers] = useState<Counterparty[]>([]);
  const [shipmentRegistryVersion, setShipmentRegistryVersion] = useState(0);
  const [transportationRegistryVersion, setTransportationRegistryVersion] = useState(0);
  const invalidateShipmentRegistry = useCallback(
    () => setShipmentRegistryVersion((old) => old + 1),
    [],
  );
  const invalidateTransportationRegistry = useCallback(
    () => setTransportationRegistryVersion((old) => old + 1),
    [],
  );
  const rememberCustomers = useCallback((items: Counterparty[]) => {
    setCustomers((old) => [
      ...old,
      ...items.filter((item) => !old.some((known) => known.id === item.id)),
    ]);
  }, []);
  const [route, setRoute] = useState(() => location.hash.slice(1) || '/');
  const [workspaces, setWorkspaces] = useState<WorkspaceTab[]>(() => openWorkspace([], route));
  const activeWorkspace = workspaceId(route);
  const localizedTabs = useMemo(
    () =>
      workspaces.map((entry) => ({
        ...entry,
        title: localizedWorkspaceTitle(entry, locale),
      })),
    [workspaces, locale],
  );
  const [restoredFor, setRestoredFor] = useState('');
  const storageKey = session ? `broker:workspaces:${session.subject}` : '';
  useEffect(() => {
    if (!storageKey) return;
    let saved: string | null = null;
    let savedRoute: string | null = null;
    try {
      saved = sessionStorage.getItem(storageKey);
      savedRoute = sessionStorage.getItem(storageKey + ':active');
    } catch {
      /* Вкладки работают без хранилища. */
    }
    const next = initialWorkspaceRoute(location.hash.slice(1), savedRoute);
    setWorkspaces(restoreWorkspaces(saved, next));
    if (next !== (location.hash.slice(1) || '/')) location.replace('#' + next);
    setRestoredFor(storageKey);
  }, [storageKey]);
  useEffect(() => {
    if (!storageKey || restoredFor !== storageKey) return;
    try {
      sessionStorage.setItem(storageKey, JSON.stringify(workspaces));
      sessionStorage.setItem(storageKey + ':active', route);
    } catch {
      /* Недоступность хранилища не мешает менять порядок вкладок. */
    }
  }, [storageKey, restoredFor, workspaces, route]);
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
    setWorkspaces((old) => {
      if (!old.some((tab) => tab.id === id && tab.title !== title)) return old;
      return old.map((tab) => (tab.id === id ? { ...tab, title } : tab));
    });
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
      setWorkspaces(remaining);
      if (activeWorkspace === id) location.hash = remaining[Math.max(0, index - 1)]?.route ?? '/';
    });
  }
  function closeAllWorkspaces() {
    const nodes = [...workspaceNodes.current.values()];
    if (nodes.some((node) => node.querySelector('form[data-busy="true"]'))) return;
    const dirty = nodes.some((node) => node.querySelector('form[data-dirty="true"]'));
    confirmDiscard(dirty, () => {
      for (const node of nodes) {
        node.querySelectorAll<HTMLElement>('[data-draft-key]').forEach((form) => {
          try {
            sessionStorage.removeItem(form.dataset.draftKey!);
          } catch {
            /* Закрытие формы не зависит от хранилища. */
          }
        });
      }
      setWorkspaces([]);
      location.hash = '/';
    });
  }
  useEffect(() => {
    function changed() {
      if (!location.hash) {
        try {
          const pending = sessionStorage.getItem('broker:login-return');
          sessionStorage.removeItem('broker:login-return');
          if (
            pending &&
            /^\/(shipments|transportations|counterparties|contracts|service-charges|financial-labels|incoming-invoices|outgoing-invoices|receipts|disbursements)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
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
      const next = location.hash.slice(1) || '/';
      setRoute(next);
      if (next.startsWith('/transportations')) setTab('transportations');
      else if (next.startsWith('/service-charges')) setTab('service-charges');
      else if (next.startsWith('/financial-labels')) setTab('financial-labels');
      else if (next.startsWith('/incoming-invoices')) setTab('incoming-invoices');
      else if (next.startsWith('/disbursements')) setTab('disbursements');
      else if (next.startsWith('/receipts')) setTab('receipts');
      else if (next.startsWith('/outgoing-invoices')) setTab('outgoing-invoices');
      else if (next.startsWith('/contracts')) setTab('contracts');
      else if (next.startsWith('/counterparties')) setTab('counterparties');
      else if (next.startsWith('/shipments')) setTab('shipments');
      else setTab('home');
    }
    changed();
    window.addEventListener('hashchange', changed);
    return () => window.removeEventListener('hashchange', changed);
  }, []);
  useEffect(() => {
    const current = workspaces.find((entry) => entry.id === activeWorkspace);
    document.title = `${current ? localizedWorkspaceTitle(current, locale) : translate('workspace.title')} · Broker`;
  }, [activeWorkspace, workspaces, locale]);
  const [navigationPinned, setNavigationPinned] = useState(() => {
    try {
      return localStorage.getItem('broker:navigation-pinned') === 'true';
    } catch {
      return false;
    }
  });
  const [notice, setNotice] = useState('');
  const error = useCallback(
    (value: unknown) => {
      if (value instanceof ApiError && value.status === 401) setSession(null);
      setNotice(
        value instanceof Error
          ? value.message +
              (value instanceof ApiError && value.traceId ? ` · Трейс: ${value.traceId}` : '')
          : 'Ошибка запроса',
      );
    },
    [setSession],
  );
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
        <p>{translate('login.opening')}</p>
      </div>
    );
  if (!session)
    return (
      <div className="login">
        <Brand />
        <h1>{translate('login.title')}</h1>
        <p>{translate('login.description')}</p>
        <a
          className="primary"
          href={`/auth/login?theme=${initialThemePreference()}&language=${locale}`}
          onClick={() => {
            try {
              sessionStorage.setItem('broker:login-return', location.hash.slice(1));
            } catch {
              /* Сохранение адреса не блокирует вход. */
            }
          }}
        >
          {translate('login.signIn')}
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
            aria-label={
              navigationPinned ? translate('workspace.unpin') : translate('workspace.pin')
            }
            aria-pressed={navigationPinned}
            title={navigationPinned ? translate('workspace.unpin') : translate('workspace.pin')}
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
        <span className="nav-label">{translate('workspace.title')}</span>
        <nav aria-label={translate('workspace.sections')}>
          <button
            aria-label={translate('nav.shipments')}
            title={translate('nav.shipments')}
            aria-current={tab === 'shipments' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/shipments';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▣
            </span>{' '}
            <span className="navigation-text">{translate('nav.shipments')}</span>
          </button>
          <button
            aria-label={translate('nav.transportations')}
            title={translate('nav.transportations')}
            aria-current={tab === 'transportations' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/transportations';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ⇄
            </span>{' '}
            <span className="navigation-text">{translate('nav.transportations')}</span>
          </button>
          <button
            aria-label={translate('nav.counterparties')}
            title={translate('nav.counterparties')}
            aria-current={tab === 'counterparties' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/counterparties';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▤
            </span>{' '}
            <span className="navigation-text">{translate('nav.counterparties')}</span>
          </button>
          <button
            aria-label={translate('nav.contracts')}
            title={translate('nav.contracts')}
            aria-current={tab === 'contracts' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/contracts';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ▧
            </span>{' '}
            <span className="navigation-text">{translate('nav.contracts')}</span>
          </button>
          {session.permissions.includes('finance.read_payments') &&
            (['receipts', 'disbursements'] as const).map((section) => (
              <button
                key={section}
                aria-label={
                  section === 'receipts'
                    ? translate('nav.receipts')
                    : translate('nav.disbursements')
                }
                title={
                  section === 'receipts'
                    ? translate('nav.receipts')
                    : translate('nav.disbursements')
                }
                aria-current={tab === section ? 'page' : undefined}
                onClick={() => {
                  location.hash = `/${section}`;
                }}
              >
                <span className="navigation-icon" aria-hidden="true">
                  {section === 'receipts' ? '↓' : '↑'}
                </span>
                <span className="navigation-text">
                  {section === 'receipts'
                    ? translate('nav.receipts')
                    : translate('nav.disbursements')}
                </span>
              </button>
            ))}
          {session.permissions.includes('finance.read_invoices') &&
            (['incoming', 'outgoing'] as const).map((direction) => (
              <button
                key={direction}
                aria-label={
                  direction === 'incoming'
                    ? translate('nav.incomingInvoices')
                    : translate('nav.outgoingInvoices')
                }
                title={
                  direction === 'incoming'
                    ? translate('nav.incomingInvoices')
                    : translate('nav.outgoingInvoices')
                }
                aria-current={tab === `${direction}-invoices` ? 'page' : undefined}
                onClick={() => {
                  location.hash = `/${direction}-invoices`;
                }}
              >
                <span className="navigation-icon" aria-hidden="true">
                  {direction === 'incoming' ? '↙' : '↗'}
                </span>
                <span className="navigation-text">
                  {direction === 'incoming'
                    ? translate('nav.incomingInvoices')
                    : translate('nav.outgoingInvoices')}
                </span>
              </button>
            ))}
          {session.permissions.includes('finance.read_service_charges') && (
            <button
              aria-label={translate('nav.serviceCharges')}
              title={translate('nav.serviceCharges')}
              aria-current={tab === 'service-charges' ? 'page' : undefined}
              onClick={() => {
                location.hash = '/service-charges';
              }}
            >
              <span className="navigation-icon" aria-hidden="true">
                ≡
              </span>
              <span className="navigation-text">{translate('nav.serviceCharges')}</span>
            </button>
          )}
          <button
            aria-label={translate('nav.financialLabels')}
            title={translate('nav.financialLabels')}
            aria-current={tab === 'financial-labels' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/financial-labels';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              ₽
            </span>{' '}
            <span className="navigation-text">{translate('nav.financialLabels')}</span>
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
            <button onClick={() => void logout()}>{translate('action.logout')}</button>
          </div>
        </div>
      </aside>
      <main>
        <header>
          <span>{translate('workspace.operations')}</span>
          <div className="header-tools">
            <span className="environment">Broker / {translate('workspace.title')}</span>
            <LanguageControl />
            <ThemeControl />
          </div>
        </header>
        <WorkspaceTabs
          tabs={localizedTabs}
          active={activeWorkspace}
          onMove={(source, target, side) =>
            setWorkspaces((old) => moveWorkspace(old, source, target, side))
          }
          onClose={closeWorkspace}
          onCloseAll={closeAllWorkspaces}
        />
        <div
          className={`workspace-content${workspaces.length ? '' : ' without-tabs'}${route === '/' ? ' home-screen' : ''}`}
        >
          {route === '/' && <WorkspaceHome permissions={session.permissions} />}
          <ShipmentRegistry
            session={session}
            active={route === '/shipments'}
            refreshVersion={shipmentRegistryVersion}
            customers={customers}
            onCustomersLoaded={rememberCustomers}
            onError={error}
          />
          <TransportationRegistry
            session={session}
            active={route === '/transportations'}
            refreshVersion={transportationRegistryVersion}
            onError={error}
          />
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
                {entry.route.startsWith('/receipts') || entry.route.startsWith('/disbursements') ? (
                  <PaymentsPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancelNew={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = entry.route.split('/').slice(0, 2).join('/');
                    }}
                    onSaved={(value) => {
                      const next = `/${value.kind === 'receipt' ? 'receipts' : 'disbursements'}/${value.id}`;
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
                ) : entry.route.startsWith('/service-charges/') ? (
                  <ServiceChargePage
                    id={entry.route.split('/')[2]}
                    session={session}
                    customers={customers}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onClose={() => {
                      setWorkspaces((old) => old.filter((tab) => tab.id !== entry.id));
                      location.hash = '/service-charges';
                    }}
                  />
                ) : entry.route.startsWith('/service-charges') ? (
                  <ServiceCharges
                    scope={{ shipment_id: null, transportation_id: null }}
                    session={session}
                    customers={customers}
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
                    onUpdate={invalidateShipmentRegistry}
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
                      setShipmentRegistryVersion((old) => old + 1);
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
                        replaceWorkspace(old, entry.id, next, transportationTitle(value, locale)),
                      );
                      location.hash = next;
                      setTransportationRegistryVersion((old) => old + 1);
                    }}
                  />
                ) : (
                  <TransportationPage
                    onChanged={invalidateTransportationRegistry}
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
