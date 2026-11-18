export const workspaceGroups = [
  {
    title: 'home.salesGroup',
    items: [
      {
        route: '/sales',
        icon: '▤',
        title: 'sales.funnel',
        description: 'home.sales',
        permission: 'sales.read',
      },
      {
        route: '/inquiries',
        icon: '▤',
        title: 'sales.inquiries',
        description: 'home.inquiries',
        permission: 'sales.read',
      },
    ],
  },
  {
    title: 'home.logistics',
    items: [
      {
        route: '/shipments',
        icon: '▣',
        title: 'nav.shipments',
        description: 'home.shipments',
        permission: 'shipments.read',
      },
      {
        route: '/transportations',
        icon: '⇄',
        title: 'nav.transportations',
        description: 'home.transportations',
        permission: 'transportations.read',
      },
    ],
  },
  {
    title: 'home.finance',
    items: [
      {
        route: '/service-charges',
        icon: '≡',
        title: 'nav.serviceCharges',
        description: 'home.serviceCharges',
        permission: 'finance.read_service_charges',
      },
      {
        route: '/outgoing-invoices',
        icon: '↗',
        title: 'nav.outgoingInvoices',
        description: 'home.outgoingInvoices',
        permission: 'finance.read_invoices',
      },
      {
        route: '/incoming-invoices',
        icon: '↙',
        title: 'nav.incomingInvoices',
        description: 'home.incomingInvoices',
        permission: 'finance.read_invoices',
      },
      {
        route: '/receipts',
        icon: '↓',
        title: 'nav.receipts',
        description: 'home.receipts',
        permission: 'finance.read_payments',
      },
      {
        route: '/disbursements',
        icon: '↑',
        title: 'nav.disbursements',
        description: 'home.disbursements',
        permission: 'finance.read_payments',
      },
    ],
  },
  {
    title: 'home.directories',
    items: [
      {
        route: '/counterparties',
        icon: '▤',
        title: 'nav.counterparties',
        description: 'home.counterparties',
        permission: 'counterparties.read',
      },
      {
        route: '/contracts',
        icon: '▧',
        title: 'nav.contracts',
        description: 'home.contracts',
        permission: 'contracts.read',
      },
      {
        route: '/financial-labels',
        icon: '₽',
        title: 'nav.financialLabels',
        description: 'home.financialLabels',
        permission: 'finance.read_financial_labels',
      },
    ],
  },
] as const;

export const workspaceSections = workspaceGroups.flatMap((group) => [...group.items]);
export const sectionRegistry = (route: string) =>
  ({
    '/financial-labels': 'financial_labels',
    '/receipts': 'receipt-payments',
    '/disbursements': 'disbursement-payments',
  })[route] ?? route.slice(1);
export function sectionForCard(route: string, permissions: string[]) {
  return workspaceSections.find(
    (section) =>
      permissions.includes(section.permission) &&
      new RegExp(`^${section.route}/[0-9a-f-]{36}(?:/(?:edit|route))?$`, 'i').test(route),
  );
}

export function workspaceSection(route: string) {
  const path = route.split('?')[0];
  const resource = path.split('/')[1];
  const section =
    resource === 'rfqs' ? 'inquiries' : resource === 'bank-accounts' ? 'counterparties' : resource;
  return workspaceSections.find((entry) => entry.route === `/${section}`);
}

export function canOpenWorkspace(route: string, permissions: string[]) {
  if (route === '/') return true;
  const section = workspaceSection(route);
  return !!section && permissions.includes(section.permission);
}
