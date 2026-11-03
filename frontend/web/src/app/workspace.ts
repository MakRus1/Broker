import { translate, type Locale } from '../../../shared/i18n/catalog';
export type WorkspaceTab = { id: string; route: string; title: string };
export function isWorkspaceRoute(route: string): boolean {
  return /^\/(shipments|transportations|counterparties|contracts|service-charges|financial-labels|incoming-invoices|outgoing-invoices|receipts|disbursements)(?:\/(?:new(?:\/[0-9a-f-]{36})?|[0-9a-f-]{36})(?:\/(?:route|edit))?)?$/i.test(
    route,
  );
}

export function initialWorkspaceRoute(current: string, saved: string | null): string {
  if (current && current !== '/') return current;
  return saved && isWorkspaceRoute(saved) ? saved : '/';
}

export function workspaceId(route: string) {
  return route.replace(/\/(edit|route)$/, '');
}
export function workspaceTitle(route: string, locale: Locale = 'ru') {
  if (/^\/shipments\/new(?:\/|$)/.test(route)) return translate(locale, 'entity.newShipment');
  if (route === '/service-charges') return translate(locale, 'nav.serviceCharges');
  if (route.startsWith('/service-charges/')) return translate(locale, 'entity.serviceCharge');
  if (route === '/financial-labels') return translate(locale, 'nav.financialLabels');
  if (route === '/financial-labels/new') return translate(locale, 'entity.newFinancialLabel');
  if (route.startsWith('/financial-labels/')) return translate(locale, 'entity.financialLabel');
  if (route === '/incoming-invoices') return translate(locale, 'nav.incomingInvoices');
  if (route === '/outgoing-invoices') return translate(locale, 'nav.outgoingInvoices');
  if (/^\/(incoming|outgoing)-invoices\/new(?:\/|$)/.test(route))
    return translate(locale, 'entity.newInvoice');
  if (/^\/(incoming|outgoing)-invoices\//.test(route)) return translate(locale, 'entity.invoice');
  if (route === '/disbursements') return translate(locale, 'nav.disbursements');
  if (route === '/disbursements/new') return translate(locale, 'entity.newDisbursement');
  if (route.startsWith('/disbursements/')) return translate(locale, 'entity.disbursement');
  if (route === '/receipts') return translate(locale, 'nav.receipts');
  if (route === '/receipts/new') return translate(locale, 'entity.newReceipt');
  if (route.startsWith('/receipts/')) return translate(locale, 'entity.receipt');
  if (route === '/contracts') return translate(locale, 'nav.contracts');
  if (route === '/contracts/new') return translate(locale, 'entity.newContract');
  if (route.startsWith('/contracts/')) return translate(locale, 'entity.contract');
  if (route === '/shipments') return translate(locale, 'nav.shipments');
  if (route.startsWith('/transportations/new/'))
    return translate(locale, 'entity.newTransportation');
  if (route === '/transportations') return translate(locale, 'nav.transportations');
  if (route === '/counterparties') return translate(locale, 'nav.counterparties');
  if (route === '/counterparties/new') return translate(locale, 'entity.newCounterparty');
  if (route.startsWith('/shipments/')) return translate(locale, 'field.shipment');
  if (route.startsWith('/transportations/')) return translate(locale, 'entity.transportation');
  return translate(locale, 'entity.counterparty');
}
export function openWorkspace(tabs: WorkspaceTab[], route: string): WorkspaceTab[] {
  if (route === '/' || !route) return tabs;
  const id = workspaceId(route);
  const existing = tabs.find((tab) => tab.id === id);
  if (existing) return tabs.map((tab) => (tab.id === id ? { ...tab, route } : tab));
  return [...tabs, { id, route, title: workspaceTitle(route) }];
}

export function replaceWorkspace(
  tabs: WorkspaceTab[],
  previous: string,
  route: string,
  title: string,
): WorkspaceTab[] {
  const id = workspaceId(route);
  return tabs
    .filter((tab) => tab.id === previous || tab.id !== id)
    .map((tab) => (tab.id === previous ? { id, route, title } : tab));
}

export function moveWorkspace(
  tabs: WorkspaceTab[],
  source: string,
  target: string,
  side: 'before' | 'after',
): WorkspaceTab[] {
  if (source === target) return tabs;
  const moving = tabs.find((tab) => tab.id === source);
  if (!moving || !tabs.some((tab) => tab.id === target)) return tabs;
  const result = tabs.filter((tab) => tab.id !== source);
  const index = result.findIndex((tab) => tab.id === target);
  result.splice(index + (side === 'after' ? 1 : 0), 0, moving);
  return result;
}

export function restoreWorkspaces(raw: string | null, route: string): WorkspaceTab[] {
  try {
    const saved: unknown = JSON.parse(raw ?? 'null');
    if (!Array.isArray(saved)) return openWorkspace([], route);
    const tabs: WorkspaceTab[] = [];
    for (const value of saved) {
      if (
        !value ||
        typeof value.route !== 'string' ||
        typeof value.title !== 'string' ||
        !isWorkspaceRoute(value.route)
      )
        continue;
      const id = workspaceId(value.route);
      if (!tabs.some((tab) => tab.id === id))
        tabs.push({ id, route: value.route, title: value.title });
    }
    return openWorkspace(tabs, route);
  } catch {
    return openWorkspace([], route);
  }
}

export function localizedWorkspaceTitle(tab: WorkspaceTab, locale: Locale): string {
  return tab.title === workspaceTitle(tab.route) ? workspaceTitle(tab.route, locale) : tab.title;
}

export function rememberWorkspace(history: readonly string[], id: string): string[] {
  return [...history.filter((entry) => entry !== id), id];
}

export function workspaceAfterClose(
  tabs: readonly WorkspaceTab[],
  closed: string,
  history: readonly string[],
): string {
  const remaining = tabs.filter((tab) => tab.id !== closed);
  for (let index = history.length - 1; index >= 0; --index) {
    const previous = remaining.find((tab) => tab.id === history[index]);
    if (previous) return previous.route;
  }
  const index = tabs.findIndex((tab) => tab.id === closed);
  return remaining[Math.max(0, index - 1)]?.route ?? '/';
}
