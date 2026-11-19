import { Chevron } from '../../../shared/ui/Select';
import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import { registryValueLabel } from '../../registry/registryLabels';
import type { Rfq } from './types';
import { RfqComparison } from './RfqComparison';
import { quotationLineKey } from './quotationComparison';

export type QuoteSelection = {
  selected: ReadonlySet<string>;
  added: ReadonlySet<string>;
  calculationDate: string;
  onToggle: (keys: string[], checked: boolean) => void;
};
export function RfqDetails({
  value,
  showRoute = true,
  selection,
}: {
  value: Rfq;
  showRoute?: boolean;
  selection?: QuoteSelection;
}) {
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
          <RfqComparison value={value} />
          {value.input.quotes.map((quote) => {
            const expired =
              !!selection && !!quote.valid_until && quote.valid_until < selection.calculationDate;
            const available = quote.lines
              .map((line) => quotationLineKey(value, quote, line))
              .filter((key) => !selection?.added.has(key));
            return (
              <details
                key={quote.id}
                className="rfq-quotation"
                open={value.input.quotes.length === 1}
              >
                <summary>
                  <Chevron />
                  <strong>{quote.supplier_name}</strong>
                  <span>{quote.title}</span>
                  <span className="rfq-service-count">
                    {translate('rfq.serviceCount', { count: quote.lines.length })}
                  </span>
                </summary>
                <div className="rfq-quotation-body">
                  {selection &&
                    (expired ? (
                      <p className="error">{translate('rfq.expired')}</p>
                    ) : (
                      <label className="rfq-select-all">
                        <input
                          type="checkbox"
                          disabled={!available.length}
                          checked={
                            available.length > 0 &&
                            available.every((key) => selection.selected.has(key))
                          }
                          onChange={(event) => selection.onToggle(available, event.target.checked)}
                        />
                        {translate('rfq.selectServices')}
                      </label>
                    ))}
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
                          {selection && <th>{translate('rfq.selection')}</th>}
                          {(
                            ['proposal.service', 'proposal.quantity', 'proposal.price'] as const
                          ).map((key) => (
                            <th key={key}>{translate(key)}</th>
                          ))}
                        </tr>
                      </thead>
                      <tbody>
                        {quote.lines.map((line) => (
                          <tr
                            key={line.id}
                            className={
                              selection?.added.has(quotationLineKey(value, quote, line))
                                ? 'rfq-line-added'
                                : undefined
                            }
                          >
                            {selection && (
                              <td>
                                {selection.added.has(quotationLineKey(value, quote, line)) ? (
                                  <span className="rfq-added">✓ {translate('rfq.added')}</span>
                                ) : (
                                  <input
                                    type="checkbox"
                                    aria-label={translate('rfq.selectService', {
                                      service: line.label_name ?? line.label_id,
                                      quote: quote.title,
                                    })}
                                    disabled={expired}
                                    checked={
                                      !expired &&
                                      selection.selected.has(quotationLineKey(value, quote, line))
                                    }
                                    onChange={(event) =>
                                      selection.onToggle(
                                        [quotationLineKey(value, quote, line)],
                                        event.target.checked,
                                      )
                                    }
                                  />
                                )}
                              </td>
                            )}
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
            );
          })}
        </div>
      )}
    </>
  );
}
