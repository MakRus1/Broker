import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import { quotationTotals } from './quotationComparison';
import type { Rfq } from './types';

export function RfqComparison({ value }: { value: Rfq }) {
  const { translate, locale } = useLocalization();
  return (
    <div className="table-scroll">
      <table className="rfq-comparison rfq-options">
        <caption>{translate('rfq.compare')}</caption>
        <thead>
          <tr>
            <th>{translate('proposal.supplier')}</th>
            <th>{translate('rfq.services')}</th>
            <th>{translate('proposal.deliveryDays')}</th>
            <th>{translate('proposal.validUntil')}</th>
            <th>{translate('finance.net')}</th>
            <th>{translate('proposal.vat')}</th>
            <th>{translate('proposal.total')}</th>
          </tr>
        </thead>
        <tbody>
          {value.input.quotes.map((quote) => {
            const totals = quotationTotals(quote.lines);
            return (
              <tr key={quote.id}>
                <td>
                  <strong>{quote.supplier_name}</strong>
                  <small>{quote.title}</small>
                </td>
                <td>{quote.lines.map((line) => line.label_name ?? line.label_id).join(', ')}</td>
                <td>{quote.delivery_days ?? '—'}</td>
                <td>
                  {quote.valid_until
                    ? new Date(`${quote.valid_until}T12:00:00`).toLocaleDateString(locale)
                    : '—'}
                </td>
                {(['net', 'vat', 'total'] as const).map((field) => (
                  <td key={field} className="rfq-amount">
                    {totals.map((total) => (
                      <div key={total.currency}>
                        {formatFixed(total[field], 2)} {total.currency}
                      </div>
                    ))}
                  </td>
                ))}
              </tr>
            );
          })}
        </tbody>
      </table>
      <p className="hint rfq-comparison-hint">{translate('rfq.compareHint')}</p>
    </div>
  );
}
