import { useRegistryPage } from '../../registry/useRegistryPage';
import { RegistryResultState } from '../../../shared/ui/RegistryExperience';
import { useLocalization } from '../../localization/Localization';
import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useCallback, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { useColumnFilters } from '../../registry/RegistryColumnFilters';
import { filterRequest, scopedFilters, type RegistryScope } from '../../registry/registryFilters';
import { preferenceKey, useRegistryPreference } from '../../registry/registryPreferences';
import { initialInvoiceQuery, restoreInvoiceQuery } from '../../registry/registryQueryPreferences';
import {
  RegistryTable,
  useRegistryColumns,
  type RegistryColumn,
} from '../../registry/RegistryTable';
import { formatFixed } from '../services/serviceCharges';
import { paymentBase, type Payment, type PaymentKind } from './payments';
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
const initialPaymentQuery = { ...initialInvoiceQuery, sort: 'payment_date' };
const restorePaymentQuery = (raw: unknown) => {
  const query = restoreInvoiceQuery(raw);
  return { ...query, sort: query.sort === 'invoice_date' ? 'payment_date' : query.sort };
};
const date = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
export function PaymentRegistry({
  kind,
  active,
  session,
  scope,
  refreshVersion = 0,
}: {
  kind: PaymentKind;
  active: boolean;
  session: Session;
  scope?: RegistryScope;
  refreshVersion?: number;
}) {
  const { translate } = useLocalization();
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, `${kind}-payments${scope ? `:${scope.key}` : ''}`, 'query'),
    initialPaymentQuery,
    restorePaymentQuery,
  );
  const loadPage = useCallback(
    async (cursor: Cursor | undefined, signal: AbortSignal) => {
      setFailure('');
      const result = await request<{ items: Payment[]; next_cursor?: Cursor }>(
        '/api/v1/payments/list',
        session,
        {
          ...query,
          kind,
          filters: filterRequest(scopedFilters(query.filters, scope)),
          limit: 50,
          cursor,
        },
        signal,
      );
      return { items: result.items, next_cursor: result.next_cursor };
    },
    [query, session, kind, scope, refreshVersion],
  );
  const page = useRegistryPage({
    active,
    refreshVersion: version + refreshVersion,
    loadPage,
    onError: (error) => setFailure((error as Error).message),
  });
  const items = page.items;
  const loading = page.loading;
  const cursor = page.more;
  const more = page.loadMore;
  const base = paymentBase(kind);
  const registryRequest = { ...query, kind, filters: scopedFilters(query.filters, scope) };
  const filters = useColumnFilters({
    registry: 'payments',
    session,
    query: registryRequest,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const scopedFields = new Set(scope?.filters.map((filter) => filter.field));
  const visibleFilters = {
    ...filters,
    fields: filters.fields.filter((field) => !scopedFields.has(field.id)),
    filters: filters.filters.filter((filter) => !scopedFields.has(filter.field)),
  };
  const columns: RegistryColumn<Payment>[] = [
    {
      id: 'number',
      labelKey: 'registryField.payment',
      width: 220,
      required: true,
      sort: 'number',
      render: (item) => (
        <a className="link" href={`#${base}/${item.id}`} onClick={(e) => e.stopPropagation()}>
          {item.number}
        </a>
      ),
    },
    {
      id: 'counterparty',
      labelKey: 'registryField.counterparty_id',
      width: 260,
      filterFields: ['counterparty_id'],
      render: (item) => item.counterparty_name,
    },
    {
      id: 'own_company',
      labelKey: 'registryField.own_company',
      width: 260,
      filterFields: ['own_company_id'],
      render: (item) => item.own_company_name,
    },
    {
      id: 'payment_date',
      labelKey: 'registryField.payment_date',
      width: 180,
      sort: 'payment_date',
      render: (item) => date(item.payment_date),
    },
    {
      id: 'total',
      labelKey: 'registryField.amount',
      width: 200,
      filterFields: ['currency'],
      render: (item) => `${formatFixed(item.amount_minor, 2)} ${item.currency}`,
    },
    {
      id: 'archived',
      labelKey: 'registryField.archived',
      width: 170,
      sort: 'archived',
      render: (item) => (item.archived ? translate('filter.archived') : translate('filter.active')),
    },
  ];
  const columnState = useRegistryColumns(session.subject, `${kind}-payments`, columns, {
    active,
    query: query,
    apply: (raw) => setQuery(restorePaymentQuery(raw)),
  });
  return (
    <section className="invoice-registry registry-workbench">
      <RegistryHeading
        columns={columnState}
        title={
          scope
            ? undefined
            : kind === 'receipt'
              ? translate('nav.receipts')
              : translate('nav.disbursements')
        }
        count={page.count}
        loading={loading}
        onRefresh={() => setVersion((v) => v + 1)}
      >
        {!scope && session.permissions.includes('finance.write_payments') && (
          <a className="button primary" href={`#${base}/new`}>
            {kind === 'receipt'
              ? translate('registryPage.addReceipt')
              : translate('registryPage.addDisbursement')}
          </a>
        )}
      </RegistryHeading>
      {failure && items.length > 0 && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <RegistryTable
        pageSource={page.source}
        items={items}
        columns={columnState}
        definitions={columns}
        filters={visibleFilters}
        sort={query.sort}
        descending={query.descending}
        onSort={(sort) =>
          setQuery((old) => ({
            ...old,
            sort,
            descending: old.sort === sort ? !old.descending : false,
          }))
        }
        onOpen={(item) => {
          location.hash = `${base}/${item.id}`;
        }}
        emptyMessage={
          <RegistryResultState
            loading={loading}
            failed={Boolean(failure)}
            failureMessage={failure}
            filtered={Boolean(query.filters?.length)}
            title={translate('registryPage.noPayments')}
            onRetry={() => {
              setVersion((value) => value + 1);
            }}
            onReset={() => setQuery((old) => ({ ...old, filters: [] }))}
            create={
              !scope && session.permissions.includes('finance.write_payments')
                ? {
                    label: translate(
                      kind === 'receipt'
                        ? 'registryPage.addReceipt'
                        : 'registryPage.addDisbursement',
                    ),
                    onClick: () => {
                      location.hash = `${base}/new`;
                    },
                  }
                : undefined
            }
          />
        }
      />
      <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
    </section>
  );
}
