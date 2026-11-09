import { Chevron } from '../../../shared/ui/Select';
import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import { registryValueLabel } from '../../registry/registryLabels';
import type { Rfq } from './types';
export function RfqDetails({ value, showRoute = true }: { value: Rfq; showRoute?: boolean }) {
  const { translate, locale } = useLocalization();
  return (
    <>
      {showRoute && (
        <p className="rfq-route">
          {registryValueLabel('mode', value.route_segment.type, locale) ?? value.route_segment.type}{' '}
          · {sharedRouteDirection(value.route_segment, locale)}
        </p>
      )}
      {value.input.note && <p className="hint">{value.input.note}</p>}
      {!value.input.quotes.length ? (
        <p className="hint rfq-empty">{translate('rfq.noQuotes')}</p>
      ) : (
        <div className="rfq-quotations">
          {value.input.quotes.map((quote) => (
            <details key={quote.id} className="rfq-quotation" open>
              <summary>
                <Chevron />
                <strong>{quote.supplier_name}</strong>
                <span>{quote.title}</span>
                <span className="rfq-service-count">
                  {translate('rfq.serviceCount', { count: quote.lines.length })}
                </span>
              </summary>
              <div className="rfq-quotation-body">
                <dl className="rfq-card-meta">
                  <div>
                    <dt>{translate('proposal.validUntil')}</dt>
                    <dd>
                      {quote.valid_until
                        ? new Date(`${quote.valid_until}T12:00:00`).toLocaleDateString(locale)
                        : '—'}
                    </dd>
                  </div>
                  <div>
                    <dt>{translate('proposal.deliveryDays')}</dt>
                    <dd>{quote.delivery_days ?? '—'}</dd>
                  </div>
                </dl>
                {quote.note && <p className="hint">{quote.note}</p>}
                <div className="table-scroll">
                  <table className="rfq-comparison">
                    <thead>
                      <tr>
                        {(['proposal.service', 'proposal.quantity', 'proposal.price'] as const).map(
                          (key) => (
                            <th key={key}>{translate(key)}</th>
                          ),
                        )}
                      </tr>
                    </thead>
                    <tbody>
                      {quote.lines.map((line) => (
                        <tr key={line.id}>
                          <td>{line.label_name}</td>
                          <td>
                            {formatFixed(line.quantity_milli, 3)}{' '}
                            {registryValueLabel('unit', line.unit, locale)}
                          </td>
                          <td>
                            {formatFixed(line.price.price_minor, 2)} {line.price.currency}
                            <small className="hint">
                              {translate(
                                line.price.price_mode === 'gross'
                                  ? 'proposal.priceGross'
                                  : 'rfq.priceNet',
                              )}{' '}
                              ·{' '}
                              {line.price.vat_mode === 'exempt'
                                ? translate('ui.noVat')
                                : `${(line.price.vat_rate_basis_points ?? 0) / 100}%`}
                            </small>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            </details>
          ))}
        </div>
      )}
    </>
  );
}
