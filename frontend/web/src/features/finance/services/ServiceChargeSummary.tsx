import { RecordSection } from '../../../shared/ui/record/RecordLayout';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { useLocalization } from '../../localization/Localization';
import { registryValueLabel } from '../../registry/registryLabels';
import { financialLabelUnits, formatVatRate } from '../labels/financialLabels';
import { formatFixed, type ServiceCharge } from './serviceCharges';
import type { ServiceChargeInvoice } from '../invoices/invoices';

export function ServiceChargeSummary({
  charge,
  invoice,
}: {
  charge: ServiceCharge;
  invoice?: ServiceChargeInvoice;
}) {
  const { translate, locale } = useLocalization();
  const amounts = (net: number, vat: number, total: number, currency: string) => (
    <dl className="record-layout-details record-amounts">
      <div>
        <dt>{translate('finance.net')}</dt>
        <dd>
          {formatFixed(net, 2)} {currency}
        </dd>
      </div>
      <div>
        <dt>{translate('registryField.vat_mode')}</dt>
        <dd>
          {formatFixed(vat, 2)} {currency}
        </dd>
      </div>
      <div>
        <dt>{translate('registryField.total_minor')}</dt>
        <dd>
          <strong>
            {formatFixed(total, 2)} {currency}
          </strong>
        </dd>
      </div>
    </dl>
  );
  return (
    <div className="record-layout-content service-charge-summary">
      <RecordSection title={translate('record.parties')}>
        <dl className="record-layout-details">
          <div>
            <dt>{translate('registryField.counterparty_id')}</dt>
            <dd>
              <a className="link" href={`#/counterparties/${charge.counterparty_id}`}>
                {charge.counterparty_name}
              </a>
            </dd>
          </div>
          <div>
            <dt>{translate('registryField.contract_id')}</dt>
            <dd>
              {charge.contract_id ? (
                <a className="link" href={`#/contracts/${charge.contract_id}`}>
                  {charge.contract_number || translate('entity.contract')}
                </a>
              ) : (
                '—'
              )}
            </dd>
          </div>
          <div>
            <dt>{translate('registryField.invoice_number')}</dt>
            <dd>
              {invoice ? (
                <a className="link" href={`#/${invoice.direction}-invoices/${invoice.invoice_id}`}>
                  {translate('finance.invoiceNumberDate', {
                    number: invoice.number,
                    date: CalendarDate.fromISO(invoice.invoice_date).toDisplayString(),
                  })}
                </a>
              ) : (
                translate('ui.notSelected')
              )}
            </dd>
          </div>
        </dl>
      </RecordSection>
      <RecordSection title={translate('record.calculation')}>
        <dl className="record-layout-details">
          <div>
            <dt>{translate('registryField.service_date')}</dt>
            <dd>{CalendarDate.fromISO(charge.service_date).toDisplayString()}</dd>
          </div>
          <div>
            <dt>{translate('registryField.direction')}</dt>
            <dd>{registryValueLabel('direction', charge.direction, locale)}</dd>
          </div>
          <div>
            <dt>{translate('registryField.quantityPrice')}</dt>
            <dd>
              {formatFixed(charge.quantity_milli, 3)}{' '}
              {registryValueLabel('unit', charge.unit, locale) ?? financialLabelUnits[charge.unit]}{' '}
              × {formatFixed(charge.price_minor, 2)} {charge.currency}
            </dd>
          </div>
          <div>
            <dt>{translate('finance.priceMode')}</dt>
            <dd>{translate(charge.price_mode === 'net' ? 'finance.net' : 'finance.gross')}</dd>
          </div>
          <div>
            <dt>{translate('registryField.vat_mode')}</dt>
            <dd>
              {charge.vat_mode === 'exempt'
                ? translate('registryValue.vat_mode.exempt')
                : `${formatVatRate(charge.vat_rate_basis_points ?? 0)}%`}
            </dd>
          </div>
        </dl>
      </RecordSection>
      <RecordSection title={translate('record.serviceAmounts')}>
        {amounts(charge.net_minor, charge.vat_minor, charge.total_minor, charge.currency)}
      </RecordSection>
      {charge.currency !== charge.invoice_currency && (
        <RecordSection title={translate('record.invoiceAmounts')}>
          {amounts(
            charge.invoice_net_minor,
            charge.invoice_vat_minor,
            charge.invoice_total_minor,
            charge.invoice_currency,
          )}
          {charge.currency !== charge.invoice_currency && (
            <p className="hint">
              {translate('finance.ratePair', {
                currency: charge.currency,
                invoiceCurrency: charge.invoice_currency,
              })}
              : {formatFixed(charge.invoice_rate.rate_millionths, 6)} ·{' '}
              {CalendarDate.fromISO(charge.invoice_rate.date).toDisplayString()} ·{' '}
              {charge.invoice_rate.source === 'manual'
                ? translate('finance.manualRate')
                : translate('record.centralBank')}
            </p>
          )}
        </RecordSection>
      )}
      {(charge.note || charge.shipment_id || charge.transportation_id) && (
        <RecordSection title={translate('record.additional')}>
          <dl className="record-layout-details">
            <div className="record-layout-wide">
              <dt>{translate('shipment.note')}</dt>
              <dd>{charge.note || '—'}</dd>
            </div>
          </dl>
          {(charge.shipment_id || charge.transportation_id) && (
            <div className="record-related-links">
              {charge.shipment_id && (
                <a className="link" href={`#/shipments/${charge.shipment_id}`}>
                  {translate('forms.openShipment')}
                </a>
              )}
              {charge.transportation_id && (
                <a className="link" href={`#/transportations/${charge.transportation_id}`}>
                  {translate('finance.openTransportation')}
                </a>
              )}
            </div>
          )}
        </RecordSection>
      )}
    </div>
  );
}
