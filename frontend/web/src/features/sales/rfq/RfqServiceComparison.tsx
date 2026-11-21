import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import {
  quotationTotals,
  quotationLineKey,
  comparableQuotationServices,
  quotationServiceKey,
} from './quotationComparison';
import type { Rfq } from './types';

export type QuoteSelection = {
  selected: ReadonlySet<string>;
  added: ReadonlySet<string>;
  calculationDate: string;
  onToggle: (keys: string[], checked: boolean) => void;
};

export function RfqServiceComparison({
  value,
  selection,
}: {
  value: Rfq;
  selection: QuoteSelection;
}) {
  const { translate, locale } = useLocalization();
  const services = comparableQuotationServices(value.input.quotes);
  return (
    <div className="table-scroll rfq-service-comparison">
      <table className="rfq-comparison">
        <caption>{translate('rfq.compare')}</caption>
        <thead>
          <tr>
            <th scope="col">{translate('proposal.service')}</th>
            {value.input.quotes.map((quote) => {
              const expired = !!quote.valid_until && quote.valid_until < selection.calculationDate;
              const keys = quote.lines.map((line) => quotationLineKey(value, quote, line));
              const available = keys.filter((key) => !selection.added.has(key));
              const selectedCount = keys.filter(
                (key) => selection.added.has(key) || selection.selected.has(key),
              ).length;
              const allSelected = keys.length > 0 && selectedCount === keys.length;
              const partiallySelected = selectedCount > 0 && !allSelected;
              return (
                <th scope="col" key={quote.id}>
                  <strong>{quote.supplier_name}</strong>
                  <span>{quote.title}</span>
                  <span>
                    {translate('proposal.deliveryDays')}: {quote.delivery_days ?? '—'}
                  </span>
                  <span>
                    {translate('proposal.validUntil')}:{' '}
                    {quote.valid_until
                      ? new Date(`${quote.valid_until}T12:00:00`).toLocaleDateString(locale)
                      : '—'}
                  </span>
                  {expired ? (
                    <span className="error">{translate('rfq.expired')}</span>
                  ) : (
                    <label className="rfq-compare-select">
                      <input
                        type="checkbox"
                        disabled={!available.length}
                        checked={allSelected}
                        aria-checked={partiallySelected ? 'mixed' : allSelected}
                        ref={(input) => {
                          if (input) input.indeterminate = partiallySelected;
                        }}
                        onChange={(event) => selection.onToggle(available, event.target.checked)}
                      />
                      {translate('rfq.selectServices')}
                    </label>
                  )}
                  {quote.note && (
                    <details className="rfq-compare-note">
                      <summary>{translate('sales.note')}</summary>
                      <p>{quote.note}</p>
                    </details>
                  )}
                </th>
              );
            })}
          </tr>
        </thead>
        <tbody>
          {services.map((service) => (
            <tr key={service.key}>
              <th scope="row">
                {service.name}
                <small>
                  {formatFixed(service.quantity_milli, 3)} ·{' '}
                  {translate(`registryValue.unit.${service.unit}`)}
                </small>
              </th>
              {value.input.quotes.map((quote) => {
                const lines = service.lines.get(quote.id) ?? [];
                const expired =
                  !!quote.valid_until && quote.valid_until < selection.calculationDate;
                return (
                  <td key={quote.id}>
                    {!lines.length && <span className="hint">{translate('rfq.notIncluded')}</span>}
                    {lines.map((line) => {
                      const key = quotationLineKey(value, quote, line);
                      const added = selection.added.has(key);
                      const selected = selection.selected.has(key);
                      const total = quotationTotals([line])[0];
                      return (
                        <label
                          key={line.id}
                          className="rfq-compare-choice"
                          data-selected={selected || added}
                        >
                          <input
                            type="radio"
                            name={`quote-service:${quotationServiceKey(value.input.route_segment_id, line.label_id)}`}
                            disabled={expired || added}
                            checked={added || (!expired && selected)}
                            aria-label={translate('rfq.selectService', {
                              service: service.name,
                              quote: quote.title,
                            })}
                            onChange={(event) => selection.onToggle([key], event.target.checked)}
                          />
                          <span>
                            <strong>
                              {formatFixed(total.total, 2)} {total.currency}
                            </strong>
                            <small>
                              {translate('finance.net')}: {formatFixed(total.net, 2)}{' '}
                              {total.currency}
                            </small>
                            {added && <small className="rfq-added">{translate('rfq.added')}</small>}
                          </span>
                        </label>
                      );
                    })}
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
      <p className="hint">{translate('rfq.serviceComparisonHint')}</p>
    </div>
  );
}
