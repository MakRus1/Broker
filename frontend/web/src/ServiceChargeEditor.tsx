import { DecimalInput } from './DecimalInput';
import type { Invoice, ServiceChargeInvoice } from './invoices';
import {
  ServiceInvoiceActions,
  attachServiceInvoice,
  type ServiceInvoiceSelection,
} from './ServiceInvoiceActions';
import { ServiceExchangeFields } from './ServiceExchangeFields';
import { CalendarDate } from './CalendarDate';
import { VatRateSelect } from './VatRateSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { ServiceChargeLookup } from './ServiceChargeLookup';
import { Select } from './Select';
import { useDiscardConfirmation } from './components';
import {
  financialLabelUnits,
  parseVatRate,
  formatVatRate,
  type FinancialLabel,
} from './financialLabels';
import type { Contract } from './ContractsPage';
import {
  inputFixed,
  parseFixed,
  type ServiceChargeScope,
  type ServiceCharge,
} from './serviceCharges';

type Draft = Omit<
  ServiceCharge,
  'quantity_milli' | 'price_minor' | 'vat_rate_basis_points' | 'service_date'
> & {
  service_date: CalendarDate | null;
  manualExchange: boolean;
  exchangeValue: string;
  quantity: string;
  price: string;
  rate: string;
};
function draftOf(value: ServiceCharge): Draft {
  const { quantity_milli, price_minor, vat_rate_basis_points, ...rest } = value;
  return {
    ...rest,
    service_date: CalendarDate.fromISO(value.service_date),
    manualExchange: value.invoice_rate.source === 'manual',
    exchangeValue: inputFixed(value.invoice_rate.rate_millionths, 6),
    quantity: inputFixed(quantity_milli, 3),
    price: inputFixed(price_minor, 2),
    rate: formatVatRate(vat_rate_basis_points ?? 0),
  };
}
export function newCharge(scope: ServiceChargeScope, direction: ServiceCharge['direction']): Draft {
  const now = new Date();
  const serviceDate = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
  return draftOf({
    ...scope,
    id: crypto.randomUUID(),
    label_id: '',
    label_name: '',
    direction,
    unit: 'transportation',
    counterparty_id: '',
    counterparty_name: '',
    contract_id: null,
    contract_number: null,
    quantity_milli: 1000,
    price_minor: 0,
    price_mode: 'net',
    currency: 'RUB',
    service_date: serviceDate,
    invoice_currency: 'RUB',
    invoice_rate: { date: serviceDate, rate_millionths: 1000000, source: 'identity' },
    invoice_net_minor: 0,
    invoice_vat_minor: 0,
    invoice_total_minor: 0,
    vat_mode: 'exempt',
    vat_rate_basis_points: null,
    net_minor: 0,
    vat_minor: 0,
    total_minor: 0,
    note: null,
    archived: false,
    revision: 0,
  });
}
export function ServiceChargeEditor({
  initial,
  session,
  customers,
  currentInvoice,
  readOnly = false,
  invoice,
  onSaved,
  onCancel,
}: {
  initial: ServiceCharge | Draft;
  session: Session;
  customers: Counterparty[];
  currentInvoice?: ServiceChargeInvoice;
  readOnly?: boolean;
  invoice?: Invoice;
  onSaved: () => void;
  onCancel: () => void;
}) {
  const [draft, setDraft] = useState<Draft>(() =>
    'quantity' in initial ? initial : draftOf(initial),
  );
  const baseline = useRef(JSON.stringify(draft));
  const [invoiceSelection, setInvoiceSelection] = useState<ServiceInvoiceSelection | undefined>(
    undefined,
  );
  const [current, setCurrent] = useState(currentInvoice);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty = JSON.stringify(draft) !== baseline.current || !!invoiceSelection;
  useEffect(() => {
    const leave = (event: BeforeUnloadEvent) => {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    };
    window.addEventListener('beforeunload', leave);
    return () => window.removeEventListener('beforeunload', leave);
  }, [dirty, busy]);
  useEffect(() => {
    if (!invoice) setInvoiceSelection(undefined);
  }, [draft.counterparty_id, draft.invoice_currency, draft.contract_id, invoice]);
  function cancel() {
    if (!busy) confirmDiscard(dirty, onCancel);
  }
  function set<K extends keyof Draft>(key: K, value: Draft[K]) {
    setDraft((old) => ({ ...old, [key]: value }));
  }
  async function save(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!draft || busy || readOnly) return;
    const quantity = parseFixed(draft.quantity, 3, 10000000);
    const price = parseFixed(draft.price, 2, 100000000);
    const rate = draft.vat_mode === 'rate' ? parseVatRate(draft.rate) : null;
    if (
      !quantity ||
      price === undefined ||
      rate === undefined ||
      !draft.service_date ||
      (draft.manualExchange && !parseFixed(draft.exchangeValue, 6, 1000000000000)) ||
      !draft.label_id ||
      !draft.counterparty_id
    ) {
      setFailure('Проверьте метку, контрагента, дату услуги, количество, цену, курс и ставку НДС');
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const saved = await request<{ charge: ServiceCharge }>(
        '/api/v1/service-charges/save',
        session,
        {
          id: draft.id,
          ...(invoice && draft.revision === 0
            ? { target_invoice_id: invoice.id, target_invoice_revision: invoice.revision }
            : {}),
          shipment_id: draft.shipment_id,
          transportation_id: draft.transportation_id,
          label_id: draft.label_id,
          direction: draft.direction,
          unit: draft.unit,
          counterparty_id: draft.counterparty_id,
          contract_id: draft.contract_id,
          quantity_milli: quantity,
          price_minor: price,
          price_mode: draft.price_mode,
          currency: draft.currency,
          service_date: draft.service_date?.toString(),
          invoice_currency: draft.invoice_currency,
          manual_invoice_rate_millionths: draft.manualExchange
            ? parseFixed(draft.exchangeValue, 6, 1000000000000)
            : null,
          vat_mode: draft.vat_mode,
          vat_rate_basis_points: rate,
          note: draft.note,
          archived: draft.archived,
          expected_revision: draft.revision,
        },
      );
      if (invoiceSelection) {
        try {
          await attachServiceInvoice(saved.charge.id, invoiceSelection, session);
        } catch (error) {
          const persisted = draftOf(saved.charge);
          setDraft(persisted);
          baseline.current = JSON.stringify(persisted);

          setFailure('Услуга сохранена, но счёт не привязан: ' + (error as Error).message);
          return;
        }
      }
      setInvoiceSelection(undefined);
      onSaved();
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить услугу');
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      <form onSubmit={save} data-dirty={dirty} data-busy={busy} className="finance-charge-form">
        <h2>
          {readOnly
            ? 'Просмотр услуги'
            : draft.revision
              ? 'Редактирование услуги'
              : draft.direction === 'income'
                ? 'Новый доход'
                : 'Новый расход'}
        </h2>
        <fieldset disabled={busy || readOnly} className="record-fields">
          <ServiceChargeLookup<FinancialLabel>
            session={session}
            kind="labels"
            label="Финансовая метка · обязательно"
            required
            value={draft.label_id}
            name={draft.label_name}
            disabled={busy}
            onChange={(label) => {
              if (label)
                setDraft((old) =>
                  old
                    ? {
                        ...old,
                        label_id: label.id,
                        label_name: label.name,
                        unit: label.unit,
                        currency: label.currency,
                        vat_mode: label.vat_mode,
                        rate: formatVatRate(label.vat_rate_basis_points ?? 0),
                        contract_id: invoice?.contract_id ?? null,
                        contract_number: invoice?.contract_number ?? null,
                        ...(invoice || label.direction === 'both'
                          ? {}
                          : { direction: label.direction }),
                      }
                    : old,
                );
            }}
          />
          <Select
            disabled={!!invoice}
            label="Направление"
            value={draft.direction}
            options={[
              { value: 'income', label: 'Доход' },
              { value: 'expense', label: 'Расход' },
            ]}
            onChange={(value) => set('direction', value as ServiceCharge['direction'])}
          />
          <CustomerSelect
            session={session}
            items={customers}
            value={draft.counterparty_id}
            required
            label="Контрагент · обязательно"
            disabled={busy || !!invoice}
            onChange={(id) =>
              setDraft((old) =>
                old
                  ? { ...old, counterparty_id: id, contract_id: null, contract_number: null }
                  : old,
              )
            }
          />
          <ServiceChargeLookup<Contract>
            session={session}
            kind="contracts"
            label="Договор"
            value={draft.contract_id ?? ''}
            name={draft.contract_number ?? ''}
            counterparty={draft.counterparty_id}
            disabled={busy || !!invoice || !draft.counterparty_id}
            onChange={(contract) =>
              setDraft((old) =>
                old
                  ? {
                      ...old,
                      contract_id: contract?.id ?? null,
                      contract_number: contract?.number ?? null,
                      ...(contract ? { invoice_currency: contract.currency } : {}),
                    }
                  : old,
              )
            }
          />
          {!invoice &&
            session.permissions.includes('finance.write_invoices') &&
            !draft.archived && (
              <ServiceInvoiceActions
                service={draft}
                session={session}
                selection={invoiceSelection}
                current={current}
                onSelect={async (selection) => {
                  const previous = current;
                  if (previous) {
                    if (selection) {
                      const saved = await attachServiceInvoice(
                        draft.id,
                        selection,
                        session,
                        previous.invoice_id,
                      );
                      setCurrent({
                        invoice_id: saved.invoice.id,
                        number: saved.invoice.number,
                        invoice_date: saved.invoice.invoice_date,
                        direction: saved.invoice.direction,
                        service_charge_id: draft.id,
                        read_only: false,
                      });
                    } else {
                      await request('/api/v1/invoices/detach-service-charge', session, {
                        invoice_id: previous.invoice_id,
                        service_charge_id: draft.id,
                      });
                      setCurrent(undefined);
                    }
                    setInvoiceSelection(undefined);
                  } else setInvoiceSelection(selection);
                }}
              />
            )}
          <label>
            Количество · обязательно
            <DecimalInput
              required
              precision={3}
              value={draft.quantity}
              onChange={(e) => {
                e.target.setCustomValidity('');
                set('quantity', e.target.value);
              }}
              onBlur={(e) =>
                e.target.setCustomValidity(
                  (parseFixed(e.target.value, 3, 10000000) ?? 0) > 0
                    ? ''
                    : 'Введите количество больше нуля, до 10 000 и до 3 знаков после запятой',
                )
              }
            />
          </label>
          <Select
            label="Единица расчёта"
            value={draft.unit}
            options={Object.entries(financialLabelUnits).map(([value, label]) => ({
              value,
              label,
            }))}
            onChange={(value) => set('unit', value as ServiceCharge['unit'])}
          />
          <label>
            Цена · обязательно
            <DecimalInput
              required
              precision={2}
              value={draft.price}
              onChange={(e) => {
                e.target.setCustomValidity('');
                set('price', e.target.value);
              }}
              onBlur={(e) =>
                e.target.setCustomValidity(
                  parseFixed(e.target.value, 2, 100000000) !== undefined
                    ? ''
                    : 'Введите цену от 0 до 1 000 000, до 2 знаков после запятой',
                )
              }
            />
          </label>
          <ServiceExchangeFields
            session={session}
            disabled={busy}
            date={draft.service_date}
            currency={draft.currency}
            invoiceCurrency={draft.invoice_currency}
            invoiceCurrencyLocked={!!invoice}
            manual={draft.manualExchange}
            rate={draft.exchangeValue}
            onDate={(value) => set('service_date', value)}
            onCurrency={(value) => set('currency', value)}
            onInvoiceCurrency={(value) =>
              setDraft((old) =>
                old
                  ? { ...old, invoice_currency: value, contract_id: null, contract_number: null }
                  : old,
              )
            }
            onManual={(value) => set('manualExchange', value)}
            onRate={(value) => set('exchangeValue', value)}
          />
          <Select
            label="Цена указана"
            value={draft.price_mode}
            options={[
              { value: 'net', label: 'Без НДС' },
              { value: 'gross', label: 'С НДС' },
            ]}
            onChange={(value) => set('price_mode', value as ServiceCharge['price_mode'])}
          />
          <VatRateSelect
            value={draft.rate}
            mode={draft.vat_mode}
            disabled={busy}
            onMode={(value) => set('vat_mode', value)}
            onChange={(value) => set('rate', value)}
          />
          <label className="record-wide">
            Примечание
            <textarea
              value={draft.note ?? ''}
              onChange={(e) => set('note', e.target.value || null)}
              maxLength={4000}
            />
          </label>
          <label className="bank-archive-toggle">
            <input
              type="checkbox"
              checked={draft.archived}
              onChange={(e) => set('archived', e.target.checked)}
            />
            В архиве
          </label>
        </fieldset>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="form-actions">
          <button type="button" disabled={busy} onClick={cancel}>
            {readOnly ? 'Закрыть' : 'Отменить'}
          </button>
          {!readOnly && (
            <button className="primary" disabled={busy}>
              Сохранить
            </button>
          )}
        </div>
      </form>
      {confirmation}
    </>
  );
}
