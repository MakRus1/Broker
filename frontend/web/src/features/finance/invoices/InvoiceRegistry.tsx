import { RegistryResultState } from '../../../shared/ui/RegistryExperience';
import { registryValueLabel } from '../../registry/registryLabels';
import { useLocalization } from '../../localization/Localization';
import { InvoicePdfArchiveDialog } from './print/InvoicePdfArchiveDialog';
import { useRowSelection } from '../../registry/useRowSelection';
import { RegistrySelectionActions } from '../../registry/RegistrySelectionActions';
import { InvoicePaymentPicker } from './InvoicePaymentPicker';
import { invoicePaymentProblem, type PaymentTarget } from './invoicePayments';
import '../services/bulkServices.css';
import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useEffect, useState } from 'react';
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
import {
  invoiceRegistry,
  type Invoice,
  type InvoiceDirection,
  type InvoiceRegistryItem,
} from './invoices';
type InvoiceRow = Invoice & Omit<InvoiceRegistryItem, 'invoice'>;
const flatten = ({ invoice, ...settlement }: InvoiceRegistryItem): InvoiceRow => ({
  ...invoice,
  ...settlement,
});
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
const date = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
export function InvoiceRegistry({
  direction,
  active,
  session,
  scope,
  refreshVersion = 0,
}: {
  direction: InvoiceDirection;
  active: boolean;
  session: Session;
  scope?: RegistryScope;
  refreshVersion?: number;
}) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<InvoiceRow[]>([]);
  const selection = useRowSelection(items.map((item) => item.id));
  const chosen = items.filter((item) => selection.selected.has(item.id));
  const targets = chosen.map((item) => ({
    invoice: item,
    outstanding_minor: item.outstanding_minor,
  }));
  const [paying, setPaying] = useState<PaymentTarget[]>();
  const [menu, setMenu] = useState<{ x: number; y: number }>();
  const [notice, setNotice] = useState('');
  const canPay = session.permissions.includes('finance.write_payments');
  const canDownload =
    direction === 'outgoing' && session.permissions.includes('finance.read_invoices');
  const canSelect = canPay || canDownload;
  const [pdfInvoices, setPdfInvoices] = useState<Invoice[]>();
  const archiveProblem = chosen.length > 100 ? translate('ui.maxInvoices') : undefined;
  const problem = invoicePaymentProblem(targets, locale);
  function distribute() {
    if (problem) return;
    setMenu(undefined);
    setPaying(targets);
  }
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const base = invoiceRegistry(direction);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, `${direction}-invoices${scope ? `:${scope.key}` : ''}`, 'query'),
    initialInvoiceQuery,
    restoreInvoiceQuery,
  );
  useEffect(() => {
    selection.clear();
    setMenu(undefined);
  }, [query, session, direction, version, refreshVersion]);
  const registryRequest = { ...query, direction, filters: scopedFilters(query.filters, scope) };
  const filters = useColumnFilters({
    registry: 'invoices',
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
  useEffect(() => {
    if (!active) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setCursor(undefined);
    request<{ items: InvoiceRegistryItem[]; next_cursor?: Cursor }>(
      '/api/v1/invoices/list',
      session,
      { ...registryRequest, filters: filterRequest(registryRequest.filters), limit: 50 },
      controller.signal,
    )
      .then((r) => {
        if (!controller.signal.aborted) {
          setItems(r.items.map(flatten));
          setCursor(r.next_cursor);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setFailure(e.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [active, direction, query, session, version, scope, refreshVersion]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const r = await request<{ items: InvoiceRegistryItem[]; next_cursor?: Cursor }>(
        '/api/v1/invoices/list',
        session,
        { ...registryRequest, filters: filterRequest(registryRequest.filters), limit: 50, cursor },
      );
      setItems((old) => [...old, ...r.items.map(flatten)]);
      setCursor(r.next_cursor);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  const columns: RegistryColumn<InvoiceRow>[] = [
    {
      id: 'number',
      labelKey: 'entity.invoice',
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
      id: 'invoice_date',
      labelKey: 'registryField.invoice_date',
      width: 180,
      sort: 'invoice_date',
      render: (item) => date(item.invoice_date),
    },
    {
      id: 'due_date',
      labelKey: 'registryField.due_date',
      width: 180,
      sort: 'due_date',
      render: (item) => date(item.due_date),
    },
    {
      id: 'total',
      labelKey: 'registryField.totalWithVat',
      width: 200,
      filterFields: ['currency'],
      sort: 'total_minor',
      render: (item) => `${formatFixed(item.total_minor, 2)} ${item.currency}`,
    },
    {
      id: 'paid_minor',
      labelKey: 'registryField.paid_minor',
      width: 180,
      sort: 'paid_minor',
      filterFields: [],
      render: (item) => `${formatFixed(item.paid_minor, 2)} ${item.currency}`,
    },
    {
      id: 'outstanding_minor',
      labelKey: 'registryField.outstanding_minor',
      width: 190,
      sort: 'outstanding_minor',
      filterFields: [],
      render: (item) => `${formatFixed(item.outstanding_minor, 2)} ${item.currency}`,
    },
    {
      id: 'payment_status',
      labelKey: 'registryField.payment_status',
      width: 210,
      sort: 'payment_status',
      render: (item) => registryValueLabel('payment_status', item.payment_status, locale),
    },
    {
      id: 'overdue',
      labelKey: 'registryField.overdue',
      width: 160,
      sort: 'overdue_days',
      filterFields: ['overdue'],
      render: (item) =>
        item.overdue_days > 0 ? (
          <span className="invoice-overdue">
            {translate('ui.days', { count: item.overdue_days })}
          </span>
        ) : (
          '—'
        ),
    },
    {
      id: 'archived',
      labelKey: 'registryField.archived',
      width: 170,
      sort: 'archived',
      render: (item) => (item.archived ? translate('filter.archived') : translate('filter.active')),
    },
  ];
  const columnState = useRegistryColumns(session.subject, `${direction}-invoices`, columns);
  return (
    <section className="invoice-registry registry-workbench">
      <RegistryHeading
        columns={columnState}
        title={
          scope
            ? undefined
            : translate(direction === 'incoming' ? 'nav.incomingInvoices' : 'nav.outgoingInvoices')
        }
        count={items.length}
        loading={loading}
        onRefresh={() => setVersion((v) => v + 1)}
      >
        {!scope && session.permissions.includes('finance.write_invoices') && (
          <a className="button primary" href={`#${base}/new`}>
            {translate('registryPage.addInvoice')}
          </a>
        )}
      </RegistryHeading>
      {notice && (
        <p className="hint" role="status">
          {notice}
        </p>
      )}
      {canSelect && (
        <RegistrySelectionActions
          count={chosen.length}
          actions={[
            ...(canPay
              ? [
                  {
                    label: translate(
                      direction === 'outgoing'
                        ? 'bulk.allocateReceipt'
                        : 'bulk.allocateDisbursement',
                    ),
                    onClick: distribute,
                    problem,
                  },
                ]
              : []),
            ...(canDownload
              ? [
                  {
                    label: translate('ui.downloadZip'),
                    problem: archiveProblem,
                    onClick: () => {
                      setMenu(undefined);
                      setPdfInvoices([...chosen]);
                    },
                  },
                ]
              : []),
          ]}
          onClear={selection.clear}
          menu={menu}
          onCloseMenu={() => setMenu(undefined)}
        />
      )}
      {pdfInvoices && (
        <InvoicePdfArchiveDialog
          invoices={pdfInvoices}
          session={session}
          onClose={() => setPdfInvoices(undefined)}
        />
      )}
      {paying && (
        <InvoicePaymentPicker
          balance={paying[0]}
          balances={paying}
          session={session}
          onClose={() => setPaying(undefined)}
          onSaved={() => {
            setPaying(undefined);
            selection.clear();
            setVersion((old) => old + 1);
            setNotice(
              translate(direction === 'outgoing' ? 'bulk.receiptSaved' : 'bulk.disbursementSaved'),
            );
          }}
        />
      )}
      {failure && items.length > 0 && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <RegistryTable
        selectionColumn={
          canSelect
            ? {
                header: (
                  <input
                    type="checkbox"
                    aria-label={translate('ui.selectInvoices')}
                    checked={items.length > 0 && chosen.length === items.length}
                    ref={(node) => {
                      if (node)
                        node.indeterminate = chosen.length > 0 && chosen.length < items.length;
                    }}
                    onChange={(event) =>
                      selection.setSelected(
                        event.target.checked ? new Set(items.map((item) => item.id)) : new Set(),
                      )
                    }
                  />
                ),
                render: (item) => (
                  <input
                    type="checkbox"
                    aria-label={translate('ui.selectInvoice', { number: item.number })}
                    checked={selection.selected.has(item.id)}
                    onPointerDown={(event) => selection.start(event, item.id)}
                    onClick={(event) => {
                      event.stopPropagation();
                      if (
                        event.detail === 0 ||
                        (event.nativeEvent as PointerEvent).pointerType === 'touch'
                      )
                        selection.toggle(item.id, !selection.selected.has(item.id), event.shiftKey);
                    }}
                    onChange={() => {}}
                  />
                ),
              }
            : undefined
        }
        rowAttributes={(item) => ({
          className: 'clickable',
          tabIndex: 0,
          'data-selection-row': item.id,
          'aria-selected': selection.selected.has(item.id),
          onClick: (event) => {
            if (!(event.target as HTMLElement).closest('a,button,input'))
              location.hash = `${base}/${item.id}`;
          },
          onKeyDown: (event) => {
            if (event.target === event.currentTarget && event.key === 'Enter') {
              event.preventDefault();
              location.hash = `${base}/${item.id}`;
            }
          },
          onContextMenu: canSelect
            ? (event) => {
                if ((event.target as HTMLElement).closest('a,button,input')) return;
                event.preventDefault();
                if (!selection.selected.has(item.id)) selection.setSelected(new Set([item.id]));
                setMenu({
                  x: Math.max(8, Math.min(event.clientX, innerWidth - 320)),
                  y: Math.max(8, Math.min(event.clientY, innerHeight - 240)),
                });
              }
            : undefined,
        })}
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
            title={translate('registryPage.noInvoices')}
            onRetry={() => {
              setVersion((value) => value + 1);
            }}
            onReset={() => setQuery((old) => ({ ...old, filters: [] }))}
            create={
              !scope && session.permissions.includes('finance.write_invoices')
                ? {
                    label: translate('registryPage.addInvoice'),
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
