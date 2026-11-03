import { saveErrorMessage } from '../../../shared/forms/saveError';
import { DateInput } from '../../../shared/dates/DateInput';
import { useFieldValidation } from '../../../shared/forms/fieldValidation';
import { RecordSection, RecordActions } from '../../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../../registry/registryLabels';
import { useLocalization } from '../../localization/Localization';
import { FinancialHistory } from '../history/FinancialHistory';
import { CardNavigation } from '../../../shared/ui/CardNavigation';
import { DecimalInput } from '../../../shared/ui/DecimalInput';
import type { Invoice, ServiceChargeInvoice } from '../invoices/invoices';
import {
  ServiceInvoiceActions,
  attachServiceInvoice,
  type ServiceInvoiceSelection,
} from '../invoices/ServiceInvoiceActions';
import { ServiceExchangeFields } from './ServiceExchangeFields';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { VatRateSelect } from './VatRateSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CustomerSelect, type Counterparty } from '../../counterparties/CustomerSelect';
import { ServiceChargeLookup } from '../invoices/ServiceChargeLookup';
import { Select } from '../../../shared/ui/Select';
import { useDiscardConfirmation } from '../../../shared/ui/Dialog';
import {
  financialLabelUnits,
  parseVatRate,
  formatVatRate,
  type FinancialLabel,
} from '../labels/financialLabels';
import type { Contract } from '../../contracts/ContractsPage';
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
  showHeading = true,
  showNavigation = true,
  invoice,
  allowInvoiceSelection = true,
  onSaved,
  onCancel,
}: {
  initial: ServiceCharge | Draft;
  session: Session;
  customers: Counterparty[];
  currentInvoice?: ServiceChargeInvoice;
  readOnly?: boolean;
  showHeading?: boolean;
  showNavigation?: boolean;
  invoice?: Invoice;
  allowInvoiceSelection?: boolean;
  onSaved: (charge: ServiceCharge) => void;
  onCancel: () => void;
}) {
  const { translate, locale } = useLocalization();
  const validation = useFieldValidation();
  const [historyOpen, setHistoryOpen] = useState(false);
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
    event.stopPropagation();
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
      setFailure(translate('finance.invalidCharge'));
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
          setFailure(
            translate('finance.chargeSavedInvoiceFailed', { message: (error as Error).message }),
          );
          return;
        }
      }
      setInvoiceSelection(undefined);
      onSaved(saved.charge);
    } catch (error) {
      setFailure(saveErrorMessage(error, translate('finance.saveServiceFailed')));
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      {showNavigation && initial.revision > 0 && (
        <CardNavigation className="financial-card-navigation">
          <button type="button" aria-pressed={!historyOpen} onClick={() => setHistoryOpen(false)}>
            {translate('entity.serviceCharge')}
          </button>
          <button type="button" aria-pressed={historyOpen} onClick={() => setHistoryOpen(true)}>
            {translate('finance.history')}
          </button>
        </CardNavigation>
      )}
      {historyOpen && (
        <FinancialHistory
          kind="service-charges"
          id={initial.id}
          revision={initial.revision}
          session={session}
        />
      )}
      <form
        hidden={historyOpen}
        onSubmit={save}
        data-validation-attempted={validation.attempted}
        onInvalid={validation.onInvalid}
        onInput={validation.onInput}
        data-dirty={dirty}
        data-busy={busy}
        className="finance-charge-form document-editor"
      >
        {showHeading && (
          <h2>
            {readOnly
              ? translate('finance.viewService')
              : draft.revision
                ? translate('finance.editService')
                : draft.direction === 'income'
                  ? translate('finance.newIncome')
                  : translate('finance.newExpense')}
          </h2>
        )}
        <fieldset disabled={busy || readOnly}>
          <RecordSection title={translate('record.identification')}>
            <div className="record-fields">
              <ServiceChargeLookup<FinancialLabel>
                session={session}
                kind="labels"
                label={translate('finance.labelRequired')}
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
                label={translate('registryField.direction')}
                value={draft.direction}
                options={[
                  { value: 'income', label: translate('registryValue.direction.income') },
                  { value: 'expense', label: translate('registryValue.direction.expense') },
                ]}
                onChange={(value) => set('direction', value as ServiceCharge['direction'])}
              />
            </div>
          </RecordSection>
          <RecordSection title={translate('record.parties')}>
            <div className="record-fields">
              <CustomerSelect
                session={session}
                items={customers}
                value={draft.counterparty_id}
                required
                label={translate('finance.counterpartyRequired')}
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
                label={translate('registryField.contract_id')}
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
              {allowInvoiceSelection &&
                !invoice &&
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
            </div>
          </RecordSection>
          <RecordSection title={translate('record.calculation')}>
            <div className="record-fields">
              <label>
                {translate('finance.serviceDateRequired')}
                <DateInput
                  required
                  disabled={busy}
                  value={draft.service_date}
                  onChange={(value) => set('service_date', value)}
                />
              </label>
              <label>
                {translate('finance.quantityRequired')}
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
                        : translate('finance.quantityInvalid'),
                    )
                  }
                />
              </label>
              <Select
                label={translate('registryField.unit')}
                value={draft.unit}
                options={Object.entries(financialLabelUnits).map(([value, label]) => ({
                  value,
                  label: registryValueLabel('unit', value, locale) ?? label,
                }))}
                onChange={(value) => set('unit', value as ServiceCharge['unit'])}
              />
              <label>
                {translate('finance.priceRequired')}
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
                        : translate('finance.priceInvalid'),
                    )
                  }
                />
              </label>
            </div>
          </RecordSection>
          <RecordSection title={translate('record.currencyAndTax')}>
            <div className="record-fields">
              <ServiceExchangeFields
                session={session}
                disabled={busy}
                date={draft.service_date}
                currency={draft.currency}
                invoiceCurrency={draft.invoice_currency}
                invoiceCurrencyLocked={!!invoice}
                manual={draft.manualExchange}
                rate={draft.exchangeValue}
                onCurrency={(value) => set('currency', value)}
                onInvoiceCurrency={(value) =>
                  setDraft((old) =>
                    old
                      ? {
                          ...old,
                          invoice_currency: value,
                          contract_id: null,
                          contract_number: null,
                        }
                      : old,
                  )
                }
                onManual={(value) => set('manualExchange', value)}
                onRate={(value) => set('exchangeValue', value)}
              />
              <Select
                label={translate('finance.priceMode')}
                value={draft.price_mode}
                options={[
                  { value: 'net', label: translate('finance.net') },
                  { value: 'gross', label: translate('finance.gross') },
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
            </div>
          </RecordSection>
          <RecordSection title={translate('record.additional')}>
            <div className="record-fields">
              <label className="record-wide">
                {translate('shipment.note')}
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
                {translate('filter.archived')}
              </label>
            </div>
          </RecordSection>
        </fieldset>
        <RecordActions
          error={failure || validation.message}
          status={
            readOnly
              ? translate('record.readOnly')
              : dirty
                ? translate('shipment.unsaved')
                : initial.revision > 0
                  ? translate('shipment.revision', { revision: initial.revision })
                  : translate('record.creating')
          }
        >
          <button type="button" disabled={busy} onClick={cancel}>
            {readOnly ? translate('ui.close') : translate('action.cancel')}
          </button>
          {!readOnly && (
            <button className="primary" disabled={busy}>
              {translate('action.save')}
            </button>
          )}
        </RecordActions>
      </form>
      {confirmation}
    </>
  );
}
