import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import { registryValueLabel } from '../../registry/registryLabels';
import type { Rfq } from './types';
export function RfqDetails({ value }: { value: Rfq }) {
  const { translate, locale } = useLocalization();
  return (
    <>
      <p>
        {registryValueLabel('mode', value.route_segment.type, locale) ?? value.route_segment.type} ·{' '}
        {sharedRouteDirection(value.route_segment, locale)}
      </p>
      <p className="hint">{value.input.note}</p>
      <div className="table-scroll">
        <table className="rfq-comparison">
          <thead>
            <tr>
              {(
                [
                  'proposal.supplier',
                  'rfq.variant',
                  'proposal.service',
                  'proposal.quantity',
                  'proposal.price',
                  'proposal.validUntil',
                  'proposal.deliveryDays',
                  'rfq.conditions',
                ] as const
              ).map((key) => (
                <th key={key}>{translate(key)}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {value.input.quotes.flatMap((quote) =>
              quote.lines.map((line) => (
                <tr key={`${quote.id}:${line.id}`}>
                  <td>{quote.supplier_name}</td>
                  <td>{quote.title}</td>
                  <td>{line.label_name}</td>
                  <td>
                    {formatFixed(line.quantity_milli, 3)}{' '}
                    {registryValueLabel('unit', line.unit, locale)}
                  </td>
                  <td>
                    {formatFixed(line.price.price_minor, 2)} {line.price.currency}
                    <small className="hint">
                      {' '}
                      ·{' '}
                      {translate(
                        line.price.price_mode === 'gross'
                          ? 'proposal.priceGross'
                          : 'proposal.priceNet',
                      )}{' '}
                      ·{' '}
                      {line.price.vat_mode === 'exempt'
                        ? translate('ui.noVat')
                        : `${(line.price.vat_rate_basis_points ?? 0) / 100}%`}
                    </small>
                  </td>
                  <td>
                    {quote.valid_until
                      ? new Date(`${quote.valid_until}T12:00:00`).toLocaleDateString(locale)
                      : '—'}
                  </td>
                  <td>{quote.delivery_days ?? '—'}</td>
                  <td>{quote.note ?? '—'}</td>
                </tr>
              )),
            )}
          </tbody>
        </table>
      </div>
    </>
  );
}
