export const workspaceGroups = [
  {
    title: 'home.logistics',
    items: [
      {
        route: '/shipments',
        title: 'nav.shipments',
        description: 'home.shipments',
        permission: 'shipments.read',
      },
      {
        route: '/transportations',
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
        title: 'nav.serviceCharges',
        description: 'home.serviceCharges',
        permission: 'finance.read_service_charges',
      },
      {
        route: '/outgoing-invoices',
        title: 'nav.outgoingInvoices',
        description: 'home.outgoingInvoices',
        permission: 'finance.read_invoices',
      },
      {
        route: '/incoming-invoices',
        title: 'nav.incomingInvoices',
        description: 'home.incomingInvoices',
        permission: 'finance.read_invoices',
      },
      {
        route: '/receipts',
        title: 'nav.receipts',
        description: 'home.receipts',
        permission: 'finance.read_payments',
      },
      {
        route: '/disbursements',
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
        title: 'nav.counterparties',
        description: 'home.counterparties',
        permission: 'counterparties.read',
      },
      {
        route: '/contracts',
        title: 'nav.contracts',
        description: 'home.contracts',
        permission: 'contracts.read',
      },
      {
        route: '/financial-labels',
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
