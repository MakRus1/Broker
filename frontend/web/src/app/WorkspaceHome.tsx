const groups = [
  {
    title: 'Логистика',
    items: [
      {
        route: '/shipments',
        title: 'Поставки',
        description: 'Груз, маршрут и план перевозок',
        permission: 'shipments.read',
      },
      {
        route: '/transportations',
        title: 'Перевозки',
        description: 'Контейнеры и участки маршрута',
        permission: 'transportations.read',
      },
    ],
  },
  {
    title: 'Финансы',
    items: [
      {
        route: '/service-charges',
        title: 'Услуги',
        description: 'Доходы и расходы',
        permission: 'finance.read_service_charges',
      },
      {
        route: '/outgoing-invoices',
        title: 'Исходящие счета',
        description: 'Счета клиентам',
        permission: 'finance.read_invoices',
      },
      {
        route: '/incoming-invoices',
        title: 'Входящие счета',
        description: 'Счета поставщиков',
        permission: 'finance.read_invoices',
      },
      {
        route: '/receipts',
        title: 'Поступления',
        description: 'Входящие платежи',
        permission: 'finance.read_payments',
      },
      {
        route: '/disbursements',
        title: 'Списания',
        description: 'Исходящие платежи',
        permission: 'finance.read_payments',
      },
    ],
  },
  {
    title: 'Справочники',
    items: [
      {
        route: '/counterparties',
        title: 'Контрагенты',
        description: 'Компании, реквизиты и счета',
        permission: 'counterparties.read',
      },
      {
        route: '/contracts',
        title: 'Договоры',
        description: 'Условия работы с контрагентами',
        permission: 'contracts.read',
      },
      {
        route: '/financial-labels',
        title: 'Финансовые метки',
        description: 'Виды услуг и единицы расчёта',
        permission: 'finance.read_financial_labels',
      },
    ],
  },
];

export function WorkspaceHome({ permissions }: { permissions: string[] }) {
  const visible = groups
    .map((group) => ({
      ...group,
      items: group.items.filter((item) => permissions.includes(item.permission)),
    }))
    .filter((group) => group.items.length);
  return (
    <section className="workspace-home" aria-labelledby="workspace-home-title">
      <h1 id="workspace-home-title">Начало работы</h1>
      <p>Откройте нужный раздел — каждая форма останется под рукой во вкладках.</p>
      <div className="workspace-shortcuts">
        {visible.map((group) => (
          <section key={group.title} aria-label={group.title}>
            <h2>{group.title}</h2>
            {group.items.map((item) => (
              <a key={item.route} href={'#' + item.route}>
                <span>
                  <strong>{item.title}</strong>
                  <small>{item.description}</small>
                </span>
                <svg viewBox="0 0 24 24" aria-hidden="true">
                  <path d="m9 5 7 7-7 7" />
                </svg>
              </a>
            ))}
          </section>
        ))}
      </div>
      {!visible.length && <p>Доступные разделы появятся после назначения прав администратором.</p>}
    </section>
  );
}
