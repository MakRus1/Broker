import { useLocalization } from '../localization/Localization';
import { formatFixed } from '../finance/services/serviceCharges';
import { registryValueLabel } from '../registry/registryLabels';
import { proposalMargin, type Proposal } from './proposals';
import type { Inquiry } from './types';
import './proposalComparison.css';

export function ProposalComparison({
  items,
  inquiry,
  onOpen,
}: {
  items: Proposal[];
  inquiry: Inquiry;
  onOpen: (item: Proposal) => void;
}) {
  const { translate, locale } = useLocalization();
  const groups = new Map<
    string,
    { name: string; quantity: number; unit: string; segment?: string }
  >();
  const key = (line: Proposal['lines'][number]) =>
    JSON.stringify([
      line.input.route_segment_id ?? '',
      line.input.label_id,
      line.input.unit,
      line.input.quantity_milli,
    ]);
  for (const item of items)
    for (const line of item.lines)
      groups.set(key(line), {
        name: line.label_name,
        quantity: line.input.quantity_milli,
        unit: line.input.unit,
        segment: line.input.route_segment_id,
      });
  const segments = inquiry.route_segments.map((segment) => segment.id);
  const money = (value: number, currency: string) => `${formatFixed(value, 2)} ${currency}`;
  const missingPurchase = (item: Proposal) => item.lines.some((line) => !line.purchase);
  return (
    <section className="proposal-comparison" aria-label={translate('proposal.compare')}>
      <p className="hint">{translate('proposal.compareHint')}</p>
      <div className="proposal-table-scroll">
        <table className="proposal-table proposal-comparison-table">
          <thead>
            <tr>
              <th scope="col">{translate('proposal.compareMetric')}</th>
              {items.map((item) => (
                <th scope="col" key={item.input.id}>
                  <button
                    type="button"
                    className="proposal-comparison-open"
                    onClick={() => onOpen(item)}
                  >
                    {item.input.title}
                  </button>
                  <small>
                    {translate(`proposal.status.${item.status}`)} ·{' '}
                    {translate('sales.revision', { revision: item.revision })}
                    {inquiry.accepted_proposal_id === item.input.id &&
                      ` · ${translate('proposal.selected')}`}
                  </small>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {(['date', 'validUntil', 'deliveryDays'] as const).map((metric) => (
              <tr key={metric}>
                <th scope="row">{translate(`proposal.${metric}`)}</th>
                {items.map((item) => (
                  <td key={item.input.id}>
                    {(metric === 'date'
                      ? item.input.calculation_date
                      : metric === 'validUntil'
                        ? item.input.valid_until
                        : item.input.delivery_days) ?? '—'}
                  </td>
                ))}
              </tr>
            ))}
            {(['saleNet', 'purchaseNet', 'profit', 'margin'] as const).map((metric) => (
              <tr key={metric}>
                <th scope="row">
                  {translate(`proposal.${metric}`)}
                  <small>{translate('proposal.reporting')}</small>
                </th>
                {items.map((item) => {
                  const total = item.reporting;
                  const unavailable =
                    (metric === 'profit' || metric === 'margin') && missingPurchase(item);
                  return (
                    <td key={item.input.id}>
                      {unavailable
                        ? '—'
                        : metric === 'margin'
                          ? proposalMargin(total)
                          : money(
                              metric === 'saleNet'
                                ? total.sale.net_minor
                                : metric === 'purchaseNet'
                                  ? total.purchase.net_minor
                                  : total.sale.net_minor - total.purchase.net_minor,
                              total.currency,
                            )}
                      {missingPurchase(item) && metric === 'purchaseNet' && (
                        <small>{translate('proposal.incompletePurchase')}</small>
                      )}
                    </td>
                  );
                })}
              </tr>
            ))}
            {[...groups]
              .sort(([, a], [, b]) => {
                const position = (segment?: string) =>
                  segment && segments.includes(segment)
                    ? segments.indexOf(segment)
                    : segments.length;
                return position(a.segment) - position(b.segment);
              })
              .map(([id, group]) => {
                const matches = items.map((item) => item.lines.filter((line) => key(line) === id));
                const signatures = matches.map((lines) =>
                  JSON.stringify(
                    lines
                      .map((line) => [
                        line.sale,
                        line.input.sale.currency,
                        line.purchase,
                        line.input.purchase?.currency,
                        line.input.supplier_id,
                      ])
                      .sort((a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b))),
                  ),
                );
                const different = new Set(signatures).size > 1;
                return (
                  <tr key={id} className={different ? 'proposal-comparison-difference' : undefined}>
                    <th scope="row">
                      {group.name}
                      <small>
                        {formatFixed(group.quantity, 3)} ·{' '}
                        {registryValueLabel('unit', group.unit, locale) ?? group.unit}
                      </small>
                      {group.segment && segments.includes(group.segment) && (
                        <small>
                          {translate('proposal.compareSegment', {
                            number: segments.indexOf(group.segment) + 1,
                          })}
                        </small>
                      )}
                      {different && (
                        <small className="proposal-comparison-difference-label">
                          {translate('proposal.differs')}
                        </small>
                      )}
                    </th>
                    {matches.map((lines, index) => (
                      <td key={items[index].input.id}>
                        {lines.length ? (
                          lines.map((line) => (
                            <div key={line.input.id} className="proposal-comparison-service">
                              <strong>
                                {money(line.sale.net_minor, line.input.sale.currency)}
                              </strong>
                              <small>
                                {translate('proposal.purchase')}:{' '}
                                {line.purchase && line.input.purchase
                                  ? money(line.purchase.net_minor, line.input.purchase.currency)
                                  : '—'}
                              </small>
                              {line.supplier_name && <small>{line.supplier_name}</small>}
                            </div>
                          ))
                        ) : (
                          <span className="hint">{translate('proposal.notIncluded')}</span>
                        )}
                      </td>
                    ))}
                  </tr>
                );
              })}
            <tr>
              <th scope="row">{translate('sales.note')}</th>
              {items.map((item) => (
                <td key={item.input.id}>{item.input.note || '—'}</td>
              ))}
            </tr>
          </tbody>
        </table>
      </div>
    </section>
  );
}
