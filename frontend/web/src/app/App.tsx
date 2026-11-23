import { SidebarHints } from './SidebarHints';
import { NavigationAppearance } from './NavigationAppearance';
import { NavigationIcon } from './NavigationIcon';
import { readNavigationMode, type NavigationMode } from './navigationPreference';
import { RfqPage } from '../features/sales/rfq/RfqPage';
import { NotificationCenter, useNotifications } from '../features/notifications/Notifications';
import { GlobalSearch } from '../features/search/GlobalSearch';
import { RelatedCreationProvider, useRelatedRequests } from '../shared/forms/RelatedCreation';
import { BankAccountCreation } from '../features/counterparties/BankAccountCreation';
import { interfaceText } from '../shared/localization/interfaceText';
import { ShipmentRegistry } from '../features/shipments/ShipmentRegistryPage';
import { TransportationRegistry } from '../features/transportations/TransportationRegistryPage';
import { WorkspaceHome } from './WorkspaceHome';
import { SidebarNavigation } from './SidebarNavigation';
import { workspaceGroups, workspaceSection, canOpenWorkspace } from './workspaceSections';
import { ServiceChargePage } from '../features/finance/services/ServiceChargePage';
import { ServiceCharges } from '../features/finance/FinancePanel';
import { PaymentsPage } from '../features/finance/payments/PaymentsPage';
import { InvoicesPage } from '../features/finance/invoices/InvoicesPage';
import { transportationTitle } from '../features/transportations/transportation';
import {
  openWorkspace,
  rememberWorkspace,
  workspaceAfterClose,
  replaceWorkspace,
  workspaceId,
  localizedWorkspaceTitle,
  moveWorkspace,
  restoreWorkspaces,
  initialWorkspaceRoute,
  type WorkspaceTab,
} from './workspace';
import { WorkspaceTabs } from './WorkspaceTabs';
import { useWorkspaceScroll } from './workspaceScroll';
import { ShipmentWorkspace } from '../features/shipments/ShipmentWorkspace';
import { useDiscardConfirmation } from '../shared/ui/Dialog';
import { profileName } from '../shared/profile/profile';
import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { ApiError, request } from '../shared/api/http';
import { FinancialLabelsPage } from '../features/finance/labels/FinancialLabelsPage';
import { SalesPage } from '../features/sales/SalesPage';
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
  return (
    <RelatedCreationProvider>
      <WorkspaceApp />
    </RelatedCreationProvider>
  );
}
function WorkspaceApp() {
  const notifications = useNotifications();
  const related = useRelatedRequests()!;
  const { translate, locale, catalogVersion } = useLocalization();
  const [session, setSession] = usePreferencesSession();
  useEffect(() => {
    related.clear();
  }, [related, session?.subject, session?.organization_id]);
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
    [workspaces, locale, catalogVersion],
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
  const workspaceContent = useRef<HTMLDivElement>(null);
  useWorkspaceScroll(workspaceContent, activeWorkspace, storageKey);
  const workspaceHistory = useRef<string[]>([activeWorkspace]);
  useEffect(() => {
    workspaceHistory.current = [activeWorkspace];
  }, [storageKey]);
  const { confirmClose, confirmation } = useDiscardConfirmation();
  useLayoutEffect(() => {
    setWorkspaces((old) => openWorkspace(old, route));
  }, [route]);
  useEffect(() => {
    workspaceHistory.current = rememberWorkspace(workspaceHistory.current, activeWorkspace);
  }, [activeWorkspace]);
  const renameWorkspace = useCallback((id: string, title: string) => {
    setWorkspaces((old) => {
      if (!old.some((tab) => tab.id === id && tab.title !== title)) return old;
      return old.map((tab) => (tab.id === id ? { ...tab, title } : tab));
    });
  }, []);
  function dismissWorkspace(id: string) {
    const origin = related.cancel(id);
    const next = workspaceAfterClose(workspaces, id, workspaceHistory.current);
    workspaceHistory.current = workspaceHistory.current.filter((entry) => entry !== id);
    setWorkspaces((old) => old.filter((tab) => tab.id !== id));
    if (activeWorkspace === id) location.hash = origin ?? next;
  }
  function finishRelated(route: string, value: unknown) {
    const origin = related.finish(route, value);
    if (origin === undefined) return false;
    const id = workspaceId(route);
    workspaceHistory.current = workspaceHistory.current.filter((entry) => entry !== id);
    setWorkspaces((old) => old.filter((entry) => entry.id !== id));
    location.hash = origin;
    return true;
  }
  function cancelWorkspaceEdit(entry: { id: string; route: string }) {
    if (/\/new(?:\/|$)/.test(entry.route)) {
      dismissWorkspace(entry.id);
      return;
    }
    const next = entry.route.replace(/\/edit$/, '');
    setWorkspaces((old) => old.map((tab) => (tab.id === entry.id ? { ...tab, route: next } : tab)));
    location.hash = next;
  }
  function closeWorkspace(id: string) {
    const node = workspaceNodes.current.get(id);
    if (node?.querySelector('form[data-busy="true"]')) return;
    confirmClose(node ? [node] : [], () => dismissWorkspace(id));
  }
  function closeAllWorkspaces() {
    const nodes = [...workspaceNodes.current.values()];
    if (nodes.some((node) => node.querySelector('form[data-busy="true"]'))) return;
    confirmClose(nodes, () => {
      related.clear();
      setWorkspaces([]);
      workspaceHistory.current = [];
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
            /^\/(shipments|transportations|counterparties|bank-accounts|sales|inquiries|rfqs|contracts|service-charges|financial-labels|incoming-invoices|outgoing-invoices|receipts|disbursements)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
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
    }
    changed();
    window.addEventListener('hashchange', changed);
    return () => window.removeEventListener('hashchange', changed);
  }, []);
  useEffect(() => {
    const current = workspaces.find((entry) => entry.id === activeWorkspace);
    document.title = `${current ? localizedWorkspaceTitle(current, locale) : translate('workspace.title')} · Broker`;
  }, [activeWorkspace, workspaces, locale, catalogVersion]);
  const [navigationMode, setNavigationMode] = useState<NavigationMode>(() => readNavigationMode());
  function changeNavigationMode(mode: NavigationMode) {
    setNavigationMode(mode);
    try {
      localStorage.setItem('broker:navigation-mode', mode);
    } catch {
      /* Настройка работает до закрытия вкладки. */
    }
  }
  const [notice, setNotice] = useState('');
  const error = useCallback(
    (value: unknown) => {
      if (value instanceof ApiError && value.status === 401) setSession(null);
      const message =
        value instanceof Error
          ? value.message +
            (value instanceof ApiError && value.traceId
              ? ` · ${interfaceText('errors.trace', { id: value.traceId })}`
              : '')
          : interfaceText('errors.request');
      if (session && !(value instanceof ApiError && value.status === 401))
        notifications.publish({ id: `application:${message}`, level: 'error', message });
      else setNotice(message);
    },
    [setSession, session, notifications],
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
    <div
      className={`app navigation-${navigationMode}${navigationMode === 'expanded' ? ' navigation-pinned' : ''}`}
    >
      <aside className="sidebar" id="workspace-navigation">
        <SidebarHints />
        <div className="sidebar-heading">
          <Brand />
        </div>

        <SidebarNavigation>
          <button
            aria-label={translate('home.navigation')}
            aria-current={route === '/' ? 'page' : undefined}
            onClick={() => {
              location.hash = '/';
            }}
          >
            <span className="navigation-icon" aria-hidden="true">
              <svg
                width="18"
                height="18"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="1.7"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <path d="m3 10 9-7 9 7v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1Z" />
                <path d="M9 21v-8h6v8" />
              </svg>
            </span>
            <span className="navigation-text">{translate('home.navigation')}</span>
          </button>
          {workspaceGroups.map((group) => {
            const sections = group.items.filter((section) =>
              session.permissions.includes(section.permission),
            );
            return (
              sections.length > 0 && (
                <div className="navigation-group" key={group.title}>
                  <span className="navigation-group-label">{translate(group.title)}</span>
                  {sections.map((section) => (
                    <button
                      key={section.route}
                      aria-label={translate(section.title)}
                      aria-current={
                        workspaceSection(route)?.route === section.route ? 'page' : undefined
                      }
                      onClick={() => {
                        location.hash = section.route;
                      }}
                    >
                      <span className="navigation-icon" aria-hidden="true">
                        <NavigationIcon route={section.route} />
                      </span>
                      <span className="navigation-text">{translate(section.title)}</span>
                    </button>
                  ))}
                </div>
              )
            );
          })}
        </SidebarNavigation>
        <NavigationAppearance mode={navigationMode} onChange={changeNavigationMode} />
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
            <GlobalSearch
              key={`${session.subject}:${session.organization_id}`}
              session={session}
              tabs={localizedTabs}
              route={route}
            />
            <NotificationCenter />
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
          ref={workspaceContent}
          className={`workspace-content${workspaces.length ? '' : ' without-tabs'}${route === '/' ? ' home-screen' : ''}`}
        >
          {route === '/' && <WorkspaceHome session={session} />}
          {!canOpenWorkspace(route, session.permissions) && (
            <p className="error" role="alert">
              {translate('workspace.accessDenied')}
            </p>
          )}
          {session.permissions.includes('shipments.read') && (
            <ShipmentRegistry
              session={session}
              active={route === '/shipments'}
              refreshVersion={shipmentRegistryVersion}
              customers={customers}
              onCustomersLoaded={rememberCustomers}
              onError={error}
            />
          )}
          {session.permissions.includes('transportations.read') && (
            <TransportationRegistry
              session={session}
              active={route === '/transportations'}
              refreshVersion={transportationRegistryVersion}
              onError={error}
            />
          )}
          {workspaces
            .filter(
              (entry) =>
                entry.route !== '/shipments' &&
                entry.route !== '/transportations' &&
                canOpenWorkspace(entry.route, session.permissions),
            )
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
                {entry.route.startsWith('/bank-accounts/new/') ? (
                  <BankAccountCreation
                    route={entry.route}
                    session={session}
                    onError={error}
                    onCancel={() => dismissWorkspace(entry.id)}
                    onSaved={(value) => {
                      if (!finishRelated(entry.route, value)) dismissWorkspace(entry.id);
                    }}
                  />
                ) : entry.route.startsWith('/receipts') ||
                  entry.route.startsWith('/disbursements') ? (
                  <PaymentsPage
                    route={entry.route}
                    active={activeWorkspace === entry.id}
                    session={session}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancel={() => cancelWorkspaceEdit(entry)}
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
                    onCancel={() => cancelWorkspaceEdit(entry)}
                    onSaved={(value) => {
                      if (finishRelated(entry.route, value)) return;
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
                    onClose={() => dismissWorkspace(entry.id)}
                  />
                ) : entry.route.startsWith('/service-charges') ? (
                  <ServiceCharges
                    active={activeWorkspace === entry.id}
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
                    onCancel={() => cancelWorkspaceEdit(entry)}
                    onSaved={(value) => {
                      if (finishRelated(entry.route, value)) return;
                      const next = `/financial-labels/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.name));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/rfqs/') ? (
                  <RfqPage
                    route={entry.route}
                    session={session}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                  />
                ) : entry.route.startsWith('/sales') || entry.route.startsWith('/inquiries') ? (
                  <SalesPage
                    route={entry.route}
                    session={session}
                    active={activeWorkspace === entry.id}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    onCancel={() => cancelWorkspaceEdit(entry)}
                    onSaved={(next, title) => {
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, title));
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
                    onCancel={() => cancelWorkspaceEdit(entry)}
                    onSaved={(value) => {
                      if (finishRelated(entry.route, value)) return;
                      const next = `/contracts/${value.id}`;
                      setWorkspaces((old) => replaceWorkspace(old, entry.id, next, value.number));
                      location.hash = next;
                    }}
                  />
                ) : entry.route.startsWith('/counterparties') ? (
                  <CounterpartiesPage
                    route={entry.route}
                    onCancel={() => cancelWorkspaceEdit(entry)}
                    active={activeWorkspace === entry.id}
                    onTitle={(title) => renameWorkspace(entry.id, title)}
                    session={session}
                    onError={error}
                    onChanged={(value) =>
                      setCustomers((old) => [...old.filter((item) => item.id !== value.id), value])
                    }
                    onSaved={(value) => {
                      if (finishRelated(entry.route, value)) return;
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
                    onCancel={() => dismissWorkspace(entry.id)}
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
          <button aria-label={translate('errors.closeNotice')} onClick={() => setNotice('')}>
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
