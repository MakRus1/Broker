import { useActivationLoad } from '../../shared/api/useActivationLoad';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { openServiceCharge } from './services/ServiceChargePage';
import { ServiceChargeRegistry } from './services/ServiceChargeRegistry';
import { serviceChargeRow } from './services/serviceChargeRow';
import { ServiceChargeEditor, newCharge } from './services/ServiceChargeEditor';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { Chevron } from '../../shared/ui/Select';
import type { ServiceChargeInvoice } from './invoices/invoices';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import { useCallback, useEffect, useRef, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import type { Counterparty } from '../counterparties/CustomerSelect';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { vatLabel, financialLabelUnits } from './labels/financialLabels';
import { ServiceInvoiceActions, attachServiceInvoice } from './invoices/ServiceInvoiceActions';
import {
  formatFixed,
  type ServiceChargeScope,
  type ServiceCharge,
  type ServiceChargeSummary,
  type CurrencyTotal,
  type ReportingSummary,
} from './services/serviceCharges';

const emptySummary: ServiceChargeSummary = { own: [], transportations: [], combined: [] };
function Totals({ title, items }: { title: string; items: CurrencyTotal[] }) {
  const { translate } = useLocalization();
  return (
    <section className="finance-totals">
      <h3>{title}</h3>
      {!items.length ? (
        <p className="hint">{translate('finance.noServices')}</p>
      ) : (
        <dl>
          {items.map((item) => (
            <div key={item.currency}>
              <dt>{item.currency}</dt>
              <dd>
                <span>{translate('finance.incomeGross')}</span>
                <strong>{formatFixed(item.income_minor, 2)}</strong>
              </dd>
              <dd>
                <span>{translate('finance.expenseGross')}</span>
                <strong>{formatFixed(item.expense_minor, 2)}</strong>
              </dd>
              <dd>
                <span>{translate('finance.differenceGross')}</span>
                <strong>
                  {formatFixed(BigInt(item.income_minor) - BigInt(item.expense_minor), 2)}
                </strong>
              </dd>
            </div>
          ))}
        </dl>
      )}
    </section>
  );
}
export function ServiceCharges({
  active = true,
  scope,
  session,
  customers,
}: {
  active?: boolean;
  scope: ServiceChargeScope;
  session: Session;
  customers: Counterparty[];
}) {
  const { translate, locale } = useLocalization();
  const read = session.permissions.includes('finance.read_service_charges');
  const write = session.permissions.includes('finance.write_service_charges');
  const [items, setItems] = useState<ServiceCharge[]>([]);
  const [invoices, setInvoices] = useState<ServiceChargeInvoice[]>([]);
  const [summary, setSummary] = useState(emptySummary);
  const [reporting, setReporting] = useState<ReportingSummary>();
  const displaySummary = reporting && !reporting.missing_rates.length ? reporting.summary : summary;
  const [summaryOpen, setSummaryOpen] = useRegistryPreference(
    preferenceKey(
      session.subject,
      scope.transportation_id ? 'transportation-finances' : 'shipment-finances',
      'summary-open',
    ),
    false,
    (raw) => raw === true,
  );
  const [after, setAfter] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [archived, setArchived] = useState(false);
  const [version, setVersion] = useState(0);
  const [editing, setEditing] = useState<ServiceCharge | ReturnType<typeof newCharge>>();
  const edit = setEditing;
  useEffect(() => {
    const refresh = () => setVersion((old) => old + 1);
    window.addEventListener('finance-charges-changed', refresh);
    return () => window.removeEventListener('finance-charges-changed', refresh);
  }, []);
  const loadedScope = useRef('');
  const load = useCallback(
    async (signal: AbortSignal) => {
      if (!scope.shipment_id) return;
      setLoading(true);
      setFailure('');
      const scopeKey = JSON.stringify([
        session.subject,
        session.organization_id,
        scope.shipment_id,
        scope.transportation_id,
        archived,
      ]);
      if (loadedScope.current !== scopeKey) {
        setItems([]);
        setInvoices([]);
        setSummary(emptySummary);
        setReporting(undefined);
        setAfter(null);
      }
      await request<{
        items: ServiceCharge[];
        next_after: string | null;
        invoices: typeof invoices;
        summary: ServiceChargeSummary;
        reporting: ReportingSummary;
      }>(
        '/api/v1/service-charges/list',
        session,
        { scope, after: null, limit: 50, include_archived: archived },
        signal,
      )
        .then((result) => {
          if (!signal.aborted) {
            loadedScope.current = scopeKey;
            setItems(result.items);
            setInvoices(result.invoices);
            setSummary(result.summary);
            setReporting(result.reporting);
            setAfter(result.next_after);
          }
        })
        .finally(() => {
          if (!signal.aborted) setLoading(false);
        });
    },
    [scope.shipment_id, scope.transportation_id, session, archived, version],
  );
  useActivationLoad(active && read && Boolean(scope.shipment_id), load, (error) => {
    setFailure(error instanceof Error ? error.message : String(error));
  });
  async function more() {
    if (loading || !after) return;
    setLoading(true);
    setFailure('');
    try {
      const result = await request<{
        items: ServiceCharge[];
        next_after: string | null;
        summary: ServiceChargeSummary;
        reporting: ReportingSummary;
        invoices: typeof invoices;
      }>('/api/v1/service-charges/list', session, {
        scope,
        after,
        limit: 50,
        include_archived: archived,
      });
      setItems((old) => [
        ...old,
        ...result.items.filter((item) => !old.some((existing) => existing.id === item.id)),
      ]);
      setAfter(result.next_after);
      setInvoices((old) => [
        ...old.filter(
          (x) => !result.invoices.some((y) => y.service_charge_id === x.service_charge_id),
        ),
        ...result.invoices,
      ]);
      setSummary(result.summary);
      setReporting(result.reporting);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : translate('finance.loadServicesFailed'));
    } finally {
      setLoading(false);
    }
  }
  if (!read) return null;
  if (!scope.shipment_id && !editing)
    return (
      <ServiceChargeRegistry
        session={session}
        version={version}
        onCreate={(direction) => {
          edit(newCharge(scope, direction));
        }}
        onEdit={openServiceCharge}
      />
    );
  return (
    <section
      className={`finance-section${scope.shipment_id ? '' : ' service-charges-page'}`}
      aria-label={translate('home.finance')}
    >
      {editing ? (
        <ServiceChargeEditor
          initial={editing}
          readOnly={
            !write ||
            (scope.shipment_id
              ? invoices.find((x) => x.service_charge_id === editing.id)?.read_only
              : undefined) === true
          }
          session={session}
          customers={customers}
          currentInvoice={
            scope.shipment_id ? invoices.find((x) => x.service_charge_id === editing.id) : undefined
          }
          onCancel={() => setEditing(undefined)}
          onSaved={() => {
            setEditing(undefined);
            setVersion((v) => v + 1);
          }}
        />
      ) : (
        <>
          <div className="section-heading">
            <h2>
              {scope.shipment_id ? translate('home.finance') : translate('nav.serviceCharges')}
            </h2>
            <div className="finance-actions">
              {write && (
                <>
                  <button className="primary" onClick={() => edit(newCharge(scope, 'income'))}>
                    {translate('registryPage.addIncome')}
                  </button>
                  <button onClick={() => edit(newCharge(scope, 'expense'))}>
                    {translate('registryPage.addExpense')}
                  </button>
                </>
              )}
              <button disabled={loading} onClick={() => setVersion((old) => old + 1)}>
                {translate('action.refresh')}
              </button>
            </div>
          </div>
          {scope.shipment_id && reporting && (
            <p className={reporting.missing_rates.length ? 'error' : 'hint'}>
              {reporting.missing_rates.length
                ? translate('finance.reportingMissing', {
                    currency: reporting.currency,
                    rates: reporting.missing_rates
                      .map((item) =>
                        translate('finance.currencyDate', {
                          currency: item.currency,
                          date: CalendarDate.fromISO(item.date).toDisplayString(),
                        }),
                      )
                      .join(', '),
                  })
                : translate('finance.reportingSummary', { currency: reporting.currency })}
            </p>
          )}
          {displaySummary.combined.length > 0 && (
            <section className="finance-summary-disclosure">
              <button
                type="button"
                className="finance-summary-toggle"
                aria-expanded={summaryOpen}
                onClick={() => setSummaryOpen((old) => !old)}
              >
                <span className="finance-summary-caption">
                  <Chevron />
                  <strong>{translate('finance.summary')}</strong>
                </span>
                <span className="finance-summary-compact">
                  {displaySummary.combined.map((item) => (
                    <span key={item.currency}>
                      <strong>{item.currency}</strong>
                      <span>{translate('registryValue.direction.income')}</span>
                      <span>{formatFixed(item.income_minor, 2)}</span>
                      <span>{translate('registryValue.direction.expense')}</span>
                      <span>{formatFixed(item.expense_minor, 2)}</span>
                      <span>{translate('finance.difference')}</span>
                      <span>
                        <strong>
                          {formatFixed(BigInt(item.income_minor) - BigInt(item.expense_minor), 2)}
                        </strong>
                      </span>
                    </span>
                  ))}
                </span>
              </button>
              {summaryOpen && (
                <div className="finance-summary">
                  <Totals
                    title={
                      !scope.shipment_id
                        ? translate('finance.withoutTransportation')
                        : scope.transportation_id
                          ? translate('registryValue.unit.transportation')
                          : translate('registryField.shipment_id')
                    }
                    items={displaySummary.own}
                  />
                  {!scope.transportation_id && (
                    <>
                      <Totals
                        title={translate('nav.transportations')}
                        items={displaySummary.transportations}
                      />
                      <Totals
                        title={translate('finance.totalLabel')}
                        items={displaySummary.combined}
                      />
                    </>
                  )}
                </div>
              )}
            </section>
          )}
          {scope.shipment_id && !scope.transportation_id && (
            <p className="hint">{translate('finance.ownChargeHint')}</p>
          )}
          <div className="finance-list-toolbar">
            {scope.shipment_id ? (
              <h3>
                {scope.transportation_id
                  ? translate('nav.serviceCharges')
                  : translate('finance.shipmentServices')}
              </h3>
            ) : (
              <span className="registry-count">
                {translate('common.loaded', { count: items.length })}
              </span>
            )}
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={archived}
                onChange={(e) => setArchived(e.target.checked)}
              />
              {translate('finance.showArchived')}
            </label>
          </div>
          {failure && (
            <p role="alert" className="error">
              {failure}
            </p>
          )}
          <div className="finance-services-table-wrap">
            <table className="finance-services-table">
              <thead>
                <tr>
                  <th>{translate('finance.serviceCounterparty')}</th>
                  <th>{translate('registryField.direction')}</th>
                  <th>{translate('registryField.quantityPrice')}</th>
                  <th>{translate('finance.net')}</th>
                  <th>{translate('registryField.vat_mode')}</th>
                  <th>{translate('registryField.total_minor')}</th>
                  <th>{translate('registryField.invoice_number')}</th>
                </tr>
              </thead>
              <tbody>
                {items.map((item) => {
                  const invoice = invoices.find((x) => x.service_charge_id === item.id);
                  const editable = write && !invoice?.read_only;
                  return (
                    <tr
                      key={item.id}
                      {...serviceChargeRow(item, openServiceCharge, !editable, locale)}
                      title={
                        invoice?.read_only ? translate('finance.paidChargeReadonly') : undefined
                      }
                    >
                      <td>
                        <strong>{item.label_name}</strong>
                        <span className="service-secondary">
                          {item.counterparty_name}
                          {item.contract_number
                            ? translate('finance.contractSuffix', { number: item.contract_number })
                            : ''}
                          {item.archived ? translate('finance.archivedSuffix') : ''}
                        </span>
                        {!scope.shipment_id && (item.shipment_id || item.transportation_id) && (
                          <a
                            className="link service-secondary"
                            href={
                              item.transportation_id
                                ? `#/transportations/${item.transportation_id}`
                                : `#/shipments/${item.shipment_id}`
                            }
                          >
                            {item.transportation_id
                              ? translate('finance.openTransportation')
                              : translate('finance.openShipment')}
                          </a>
                        )}
                        {!scope.shipment_id && !item.shipment_id && (
                          <span className="service-secondary">
                            {translate('finance.standalone')}
                          </span>
                        )}
                        {item.note && (
                          <details className="service-note">
                            <summary>{translate('shipment.note')}</summary>
                            <p>{item.note}</p>
                          </details>
                        )}
                      </td>
                      <td>
                        {item.direction === 'income'
                          ? translate('registryValue.direction.income')
                          : translate('registryValue.direction.expense')}
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.quantity_milli, 3)}{' '}
                        {registryValueLabel('unit', item.unit, locale) ??
                          financialLabelUnits[item.unit]}
                        <span className="service-secondary">
                          × {formatFixed(item.price_minor, 2)} {item.currency} ·{' '}
                          {item.price_mode === 'gross'
                            ? translate('finance.grossLower')
                            : translate('finance.netLower')}
                        </span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.net_minor, 2)}
                        <span className="service-secondary">{item.currency}</span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.vat_minor, 2)}
                        <span className="service-secondary">{vatLabel(item, locale)}</span>
                      </td>
                      <td className="service-numeric">
                        <strong>{formatFixed(item.total_minor, 2)}</strong>
                        <span className="service-secondary">{item.currency}</span>
                        {item.invoice_currency !== item.currency && (
                          <span className="service-secondary">
                            {translate('finance.invoiceAmount', {
                              amount: formatFixed(item.invoice_total_minor, 2),
                              currency: item.invoice_currency,
                            })}
                            <br />
                            {translate('finance.rateDescription', {
                              rate: formatFixed(item.invoice_rate.rate_millionths, 6),
                              source:
                                item.invoice_rate.source === 'manual'
                                  ? translate('finance.manualLower')
                                  : translate('finance.centralBank'),
                              date: CalendarDate.fromISO(item.invoice_rate.date).toDisplayString(),
                            })}
                          </span>
                        )}
                      </td>
                      <td>
                        {invoice ? (
                          <a
                            className="link"
                            href={`#/${invoice.direction}-invoices/${invoice.invoice_id}`}
                          >
                            № {invoice.number}
                            <span className="service-secondary">
                              {translate('finance.invoiceDate', {
                                date: CalendarDate.fromISO(invoice.invoice_date).toDisplayString(),
                              })}
                            </span>
                          </a>
                        ) : !item.archived &&
                          session.permissions.includes('finance.write_invoices') ? (
                          <ServiceInvoiceActions
                            compact
                            service={item}
                            session={session}
                            onSelect={async (selection) => {
                              if (selection) {
                                await attachServiceInvoice(item.id, selection, session);
                                setVersion((v) => v + 1);
                              }
                            }}
                          />
                        ) : (
                          <span className="hint">{translate('ui.notChosen')}</span>
                        )}
                        {invoice?.read_only && (
                          <span className="service-secondary">
                            {translate('finance.paidReadonly')}
                          </span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          {!loading && !items.length && !failure && (
            <p className="empty-state">{translate('finance.noServicesYet')}</p>
          )}
          <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        </>
      )}
    </section>
  );
}
