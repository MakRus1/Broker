import { CalendarDate } from '../../shared/dates/CalendarDate';
import type { MessageKey } from '../../../../shared/i18n/catalog';
import type { Session } from '../../shared/api/session';
import { request } from '../../shared/api/http';
import type { Shipment } from '../shipments/types';
import type { Transportation } from '../transportations/types';
import type { Counterparty } from '../counterparties/counterparties';
import type { Contract } from '../contracts/ContractsPage';
import type { InvoiceRegistryItem } from '../finance/invoices/invoices';

const dateLabel = (value?: string) => (value ? CalendarDate.fromISO(value).toDisplayString() : '');

export interface SearchHit {
  id: string;
  title: string;
  detail: string;
  route: string;
}
export interface SearchSource {
  id: string;
  label: MessageKey;
  permission: string;
  field: string;
  sort: string;
  load: (session: Session, query: string, signal: AbortSignal) => Promise<SearchHit[]>;
}
function source<T>(
  id: string,
  label: MessageKey,
  permission: string,
  field: string,
  sort: string,
  endpoint: string,
  map: (value: T) => SearchHit,
  extra = {},
): SearchSource {
  return {
    id,
    label,
    permission,
    field,
    sort,
    async load(session, query, signal) {
      const result = await request<{ items: T[] }>(
        endpoint,
        session,
        { ...searchRequest({ id, field, sort }, query), ...extra, limit: 5 },
        signal,
      );
      return result.items.map(map);
    },
  };
}
export function searchRequest(source: Pick<SearchSource, 'id' | 'field' | 'sort'>, query: string) {
  return {
    sort: source.sort,
    descending: source.sort.endsWith('_date'),
    filters: [
      { field: source.field, query },
      ...(['shipments', 'transportations'].includes(source.id)
        ? []
        : [{ field: 'archived', values: ['false'] }]),
    ],
  };
}
export const searchSources: SearchSource[] = [
  source<Shipment>(
    'shipments',
    'nav.shipments',
    'shipments.read',
    'number',
    'shipment_date',
    '/api/v1/shipments/list',
    (v) => ({
      id: v.id,
      title: v.number,
      detail: [dateLabel(v.shipment_date), v.cargo_description].filter(Boolean).join(' · '),
      route: `/shipments/${v.id}`,
    }),
  ),
  source<Transportation>(
    'transportations',
    'nav.transportations',
    'transportations.read',
    'container_number',
    'container_number',
    '/api/v1/transportations/list',
    (v) => ({
      id: v.id,
      title: v.container_number ?? '',
      detail: [
        v.container_type_code,
        v.container_ownership,
        ...v.route_segments
          .slice(0, 2)
          .map((s) => [s.origin?.name, s.destination?.name].filter(Boolean).join(' → ')),
      ]
        .filter(Boolean)
        .join(' · '),
      route: `/transportations/${v.id}`,
    }),
  ),
  source<Counterparty>(
    'counterparties',
    'nav.counterparties',
    'counterparties.read',
    'name',
    'name',
    '/api/v1/counterparties/list',
    (v) => ({
      id: v.id,
      title: v.name,
      detail: [v.tax_id, v.country_code].filter(Boolean).join(' · '),
      route: `/counterparties/${v.id}`,
    }),
  ),
  source<Contract>(
    'contracts',
    'nav.contracts',
    'contracts.read',
    'number',
    'contract_date',
    '/api/v1/contracts/list',
    (v) => ({
      id: v.id,
      title: v.number,
      detail: [dateLabel(v.contract_date), v.currency].join(' · '),
      route: `/contracts/${v.id}`,
    }),
  ),
  ...(['outgoing', 'incoming'] as const).map((direction) =>
    source<InvoiceRegistryItem>(
      `${direction}-invoices`,
      direction === 'outgoing' ? 'nav.outgoingInvoices' : 'nav.incomingInvoices',
      'finance.read_invoices',
      'number',
      'invoice_date',
      '/api/v1/invoices/list',
      ({ invoice: v }) => ({
        id: v.id,
        title: v.number,
        detail: [dateLabel(v.invoice_date), v.counterparty_name, v.own_company_name, v.currency]
          .filter(Boolean)
          .join(' · '),
        route: `/${direction}-invoices/${v.id}`,
      }),
      { direction },
    ),
  ),
];
export function permittedSources(session: Session) {
  return searchSources.filter((source) => session.permissions.includes(source.permission));
}
export function sourceForRoute(route: string, sources: SearchSource[]) {
  return sources.find((source) =>
    new RegExp(`^/${source.id}/[0-9a-f-]{36}(?:/(?:edit|route))?$`, 'i').test(route),
  );
}
