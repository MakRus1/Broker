import { useLocalization } from '../../localization/Localization';
import { formatFixed } from '../../finance/services/serviceCharges';
import { proposalLineFinancials } from '../proposalSalesCalculation';
import { replaceQuotedServices } from './quotationImport';
import type { ProposalLine } from '../proposals';

export function QuotationChangePreview({
  before,
  incoming,
}: {
  before: ProposalLine;
  incoming: ProposalLine;
}) {
  const { translate } = useLocalization();
  const after = replaceQuotedServices([before], [incoming])[0];
  const old = proposalLineFinancials(before);
  const next = proposalLineFinancials(after);
  const money = (value: bigint | undefined, currency?: string) =>
    value === undefined ? '—' : `${formatFixed(value, 2)} ${currency ?? ''}`;
  const margin = (value: number | undefined) =>
    value === undefined ? '—' : `${formatFixed(value, 2)}%`;
  const volumeChanged =
    before.unit !== after.unit || before.quantity_milli !== after.quantity_milli;
  return (
    <div className="rfq-change-preview">
      <dl>
        <div>
          <dt>{translate('proposal.purchaseNet')}</dt>
          <dd>
            {money(old.purchase?.net, before.purchase?.currency)} →{' '}
            {money(next.purchase?.net, after.purchase?.currency)}
          </dd>
        </div>
        <div>
          <dt>{translate('proposal.profit')}</dt>
          <dd>
            {money(old.profit, before.sale.currency)} → {money(next.profit, after.sale.currency)}
          </dd>
        </div>
        <div>
          <dt>{translate('proposal.margin')}</dt>
          <dd>
            {margin(old.margin)} → {margin(next.margin)}
          </dd>
        </div>
      </dl>
      <small>{translate(volumeChanged ? 'rfq.volumeResetsSale' : 'rfq.saleRetained')}</small>
      {(old.profit === undefined || next.profit === undefined) && (
        <small>
          {translate(
            !before.purchase || !after.purchase
              ? 'proposal.profitNeedsPurchase'
              : 'proposal.profitDifferentCurrencies',
          )}
        </small>
      )}
    </div>
  );
}
