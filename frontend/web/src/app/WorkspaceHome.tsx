const groups = [
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

import { useLocalization } from '../features/localization/Localization';

export function WorkspaceHome({ permissions }: { permissions: string[] }) {
  const { translate } = useLocalization();
  const visible = groups
    .map((group) => ({
      ...group,
      items: group.items.filter((item) => permissions.includes(item.permission)),
    }))
    .filter((group) => group.items.length);
  return (
    <section className="workspace-home" aria-labelledby="workspace-home-title">
      <h1 id="workspace-home-title">{translate('home.title')}</h1>
      <p>{translate('home.hint')}</p>
      <div className="workspace-shortcuts">
        {visible.map((group) => (
          <section key={group.title} aria-label={translate(group.title)}>
            <h2>{translate(group.title)}</h2>
            {group.items.map((item) => (
              <a key={item.route} href={'#' + item.route}>
                <span>
                  <strong>{translate(item.title)}</strong>
                  <small>{translate(item.description)}</small>
                </span>
                <svg viewBox="0 0 24 24" aria-hidden="true">
                  <path d="m9 5 7 7-7 7" />
                </svg>
              </a>
            ))}
          </section>
        ))}
      </div>
      {!visible.length && <p>{translate('home.noAccess')}</p>}
    </section>
  );
}
