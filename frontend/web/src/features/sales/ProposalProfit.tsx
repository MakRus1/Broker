import { useLocalization } from '../localization/Localization';
import { formatFixed } from '../finance/services/serviceCharges';
import { proposalAmounts } from './proposalAmounts';
import type { ProposalLine } from './proposals';

export function ProposalProfit({ line }: { line: ProposalLine }) {
  const { translate, locale } = useLocalization();
  if (!line.purchase) return <p className="hint">{translate('proposal.profitNeedsPurchase')}</p>;

  const sale = proposalAmounts(line.quantity_milli, line.sale).net;
  const purchase = proposalAmounts(line.quantity_milli, line.purchase).net;
  const sameCurrency = line.sale.currency === line.purchase.currency;
  const profit = sale - purchase;
  const percentage = (base: bigint) =>
    base > 0n
      ? new Intl.NumberFormat(locale, { style: 'percent', maximumFractionDigits: 1 }).format(
          Number(profit) / Number(base),
        )
      : '—';

  return (
    <div className="proposal-live-profit">
      <dl>
        <div>
          <dt>{translate('proposal.purchaseNet')}</dt>
          <dd>
            {formatFixed(purchase, 2)} {line.purchase.currency}
          </dd>
        </div>
        <div>
          <dt>{translate('proposal.saleNet')}</dt>
          <dd>
            {formatFixed(sale, 2)} {line.sale.currency}
          </dd>
        </div>
        {sameCurrency && (
          <>
            <div>
              <dt>{translate('proposal.profit')}</dt>
              <dd>
                {formatFixed(profit, 2)} {line.sale.currency}
              </dd>
            </div>
            <div>
              <dt>{translate('proposal.markup')}</dt>
              <dd>{percentage(purchase)}</dd>
            </div>
            <div>
              <dt>{translate('proposal.margin')}</dt>
              <dd>{percentage(sale)}</dd>
            </div>
          </>
        )}
      </dl>
      {!sameCurrency ? (
        <p className="hint">{translate('proposal.profitDifferentCurrencies')}</p>
      ) : (
        <>
          <p className="hint">{translate('proposal.profitBasis')}</p>
          {profit <= 0n && (
            <p className={profit < 0n ? 'error' : 'proposal-zero-profit'} role="status">
              {translate(profit < 0n ? 'proposal.lossHint' : 'proposal.zeroProfitHint')}
            </p>
          )}
        </>
      )}
    </div>
  );
}
