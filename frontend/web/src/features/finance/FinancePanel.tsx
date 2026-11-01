import { openServiceCharge } from './services/ServiceChargePage';
import { ServiceChargeRegistry } from './services/ServiceChargeRegistry';
import { serviceChargeRow } from './services/serviceChargeRow';
import { ServiceChargeEditor, newCharge } from './services/ServiceChargeEditor';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { Chevron } from '../../shared/ui/Select';
import type { ServiceChargeInvoice } from './invoices/invoices';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import { useEffect, useState } from 'react';
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
  return (
    <section className="finance-totals">
      <h3>{title}</h3>
      {!items.length ? (
        <p className="hint">Услуг нет</p>
      ) : (
        <dl>
          {items.map((item) => (
            <div key={item.currency}>
              <dt>{item.currency}</dt>
              <dd>
                <span>Доход с НДС</span>
                <strong>{formatFixed(item.income_minor, 2)}</strong>
              </dd>
              <dd>
                <span>Расход с НДС</span>
                <strong>{formatFixed(item.expense_minor, 2)}</strong>
              </dd>
              <dd>
                <span>Разница с НДС</span>
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
  scope,
  session,
  customers,
}: {
  scope: ServiceChargeScope;
  session: Session;
  customers: Counterparty[];
}) {
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
  useEffect(() => {
    if (!read || !scope.shipment_id) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setSummary(emptySummary);
    setReporting(undefined);
    setAfter(null);
    request<{
      items: ServiceCharge[];
      next_after: string | null;
      invoices: typeof invoices;
      summary: ServiceChargeSummary;
      reporting: ReportingSummary;
    }>(
      '/api/v1/service-charges/list',
      session,
      { scope, after: null, limit: 50, include_archived: archived },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setInvoices(result.invoices);
          setSummary(result.summary);
          setReporting(result.reporting);
          setAfter(result.next_after);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [scope.shipment_id, scope.transportation_id, session, read, archived, version]);
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
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить услуги');
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
      aria-label="Финансы"
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
            <h2>{scope.shipment_id ? 'Финансы' : 'Услуги'}</h2>
            <div className="finance-actions">
              {write && (
                <>
                  <button className="primary" onClick={() => edit(newCharge(scope, 'income'))}>
                    Добавить доходную услугу
                  </button>
                  <button onClick={() => edit(newCharge(scope, 'expense'))}>
                    Добавить расходную услугу
                  </button>
                </>
              )}
              <button disabled={loading} onClick={() => setVersion((old) => old + 1)}>
                Обновить
              </button>
            </div>
          </div>
          {scope.shipment_id && reporting && (
            <p className={reporting.missing_rates.length ? 'error' : 'hint'}>
              {reporting.missing_rates.length
                ? `Пересчёт в ${reporting.currency} недоступен: нет курсов для ${reporting.missing_rates.map((item) => `${item.currency} на ${CalendarDate.fromISO(item.date).toDisplayString()}`).join(', ')}. Показаны суммы по валютам услуг.`
                : `Сводка в ${reporting.currency} · пересчёт на даты услуг`}
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
                  <strong>Сводка</strong>
                </span>
                <span className="finance-summary-compact">
                  {displaySummary.combined.map((item) => (
                    <span key={item.currency}>
                      <strong>{item.currency}</strong>
                      <span>Доход</span>
                      <span>{formatFixed(item.income_minor, 2)}</span>
                      <span>Расход</span>
                      <span>{formatFixed(item.expense_minor, 2)}</span>
                      <span>Разница</span>
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
                        ? 'Услуги без перевозки'
                        : scope.transportation_id
                          ? 'Перевозка'
                          : 'Поставка'
                    }
                    items={displaySummary.own}
                  />
                  {!scope.transportation_id && (
                    <>
                      <Totals title="Перевозки" items={displaySummary.transportations} />
                      <Totals title="Всего" items={displaySummary.combined} />
                    </>
                  )}
                </div>
              )}
            </section>
          )}
          {scope.shipment_id && !scope.transportation_id && (
            <p className="hint">
              Ниже — собственные строки поставки. Услуги перевозок редактируются в перевозках и
              учитываются в сводке один раз.
            </p>
          )}
          <div className="finance-list-toolbar">
            {scope.shipment_id ? (
              <h3>{scope.transportation_id ? 'Услуги' : 'Услуги поставки'}</h3>
            ) : (
              <span className="registry-count">Загружено: {items.length}</span>
            )}
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={archived}
                onChange={(e) => setArchived(e.target.checked)}
              />
              Показать архивные
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
                  <th>Услуга / контрагент</th>
                  <th>Направление</th>
                  <th>Количество × цена</th>
                  <th>Без НДС</th>
                  <th>НДС</th>
                  <th>Итого</th>
                  <th>Счёт</th>
                </tr>
              </thead>
              <tbody>
                {items.map((item) => {
                  const invoice = invoices.find((x) => x.service_charge_id === item.id);
                  const editable = write && !invoice?.read_only;
                  return (
                    <tr
                      key={item.id}
                      {...serviceChargeRow(item, openServiceCharge, !editable)}
                      title={
                        invoice?.read_only
                          ? 'Счёт оплачен. Услуга доступна только для чтения'
                          : undefined
                      }
                    >
                      <td>
                        <strong>{item.label_name}</strong>
                        <span className="service-secondary">
                          {item.counterparty_name}
                          {item.contract_number ? ` · Договор ${item.contract_number}` : ''}
                          {item.archived ? ' · В архиве' : ''}
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
                            {item.transportation_id ? 'К перевозке' : 'К поставке'}
                          </a>
                        )}
                        {!scope.shipment_id && !item.shipment_id && (
                          <span className="service-secondary">Самостоятельная услуга</span>
                        )}
                        {item.note && (
                          <details className="service-note">
                            <summary>Примечание</summary>
                            <p>{item.note}</p>
                          </details>
                        )}
                      </td>
                      <td>{item.direction === 'income' ? 'Доход' : 'Расход'}</td>
                      <td className="service-numeric">
                        {formatFixed(item.quantity_milli, 3)} {financialLabelUnits[item.unit]}
                        <span className="service-secondary">
                          × {formatFixed(item.price_minor, 2)} {item.currency} ·{' '}
                          {item.price_mode === 'gross' ? 'с НДС' : 'без НДС'}
                        </span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.net_minor, 2)}
                        <span className="service-secondary">{item.currency}</span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.vat_minor, 2)}
                        <span className="service-secondary">{vatLabel(item)}</span>
                      </td>
                      <td className="service-numeric">
                        <strong>{formatFixed(item.total_minor, 2)}</strong>
                        <span className="service-secondary">{item.currency}</span>
                        {item.invoice_currency !== item.currency && (
                          <span className="service-secondary">
                            В счёте: {formatFixed(item.invoice_total_minor, 2)}{' '}
                            {item.invoice_currency}
                            <br />
                            Курс {formatFixed(item.invoice_rate.rate_millionths, 6)} ·{' '}
                            {item.invoice_rate.source === 'manual' ? 'вручную' : 'ЦБ'} на{' '}
                            {CalendarDate.fromISO(item.invoice_rate.date).toDisplayString()}
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
                              от {CalendarDate.fromISO(invoice.invoice_date).toDisplayString()}
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
                          <span className="hint">Не выбран</span>
                        )}
                        {invoice?.read_only && (
                          <span className="service-secondary">Оплачен · только чтение</span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          {!loading && !items.length && !failure && (
            <p className="empty-state">Услуги пока не добавлены</p>
          )}
          <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        </>
      )}
    </section>
  );
}
