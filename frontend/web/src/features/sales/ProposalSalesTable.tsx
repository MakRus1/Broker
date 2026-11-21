import { useEffect, useRef, useState } from 'react';
import { useLocalization } from '../localization/Localization';
import { formatFixed, inputFixed, parseFixed } from '../finance/services/serviceCharges';
import { NumberField } from './ProposalFields';
import {
  proposalDraftTotals,
  proposalLineFinancials,
  salePriceForMarkup,
} from './proposalSalesCalculation';
import type { ProposalLine } from './proposals';
import './proposalSalesTable.css';

function MarkupField({
  line,
  label,
  onChange,
}: {
  line: ProposalLine;
  label: string;
  onChange: (line: ProposalLine) => void;
}) {
  const { translate } = useLocalization();
  const { markup } = proposalLineFinancials(line);
  const [text, setText] = useState(markup === undefined ? '' : inputFixed(markup, 2));
  const focused = useRef(false);
  const input = useRef<HTMLInputElement>(null);
  useEffect(() => {
    if (!focused.current) {
      setText(markup === undefined ? '' : inputFixed(markup, 2));
      input.current?.setCustomValidity('');
    }
  }, [markup]);
  return (
    <label className="proposal-compact-field">
      <span>{label}</span>
      <input
        ref={input}
        inputMode="decimal"
        value={text}
        disabled={markup === undefined}
        placeholder="—"
        onFocus={() => {
          focused.current = true;
        }}
        onBlur={() => {
          focused.current = false;
          if (input.current?.validity.valid)
            setText(markup === undefined ? '' : inputFixed(markup, 2));
        }}
        onChange={(event) => {
          const raw = event.target.value.replace(/[\s\u00a0\u202f]/g, '');
          if (!/^-?\d*(?:[.,]\d{0,2})?$/.test(raw)) return;
          setText(raw);
          const value = parseFixed(raw.replace(/^-/, ''), 2, 10000000);
          const sale =
            value === undefined
              ? undefined
              : salePriceForMarkup(line, raw.startsWith('-') ? -value : value);
          event.target.setCustomValidity(sale ? '' : translate('proposal.invalidMarkup'));
          if (sale) onChange({ ...line, sale });
        }}
      />
    </label>
  );
}

export function ProposalSalesTable({
  lines,
  name,
  onChange,
  onDetails,
  onRemove,
}: {
  lines: ProposalLine[];
  name: (line: ProposalLine) => string;
  onChange: (line: ProposalLine) => void;
  onDetails: (id: string) => void;
  onRemove: (id: string) => void;
}) {
  const { translate, locale } = useLocalization();
  const percentage = (value: number | undefined) =>
    value === undefined
      ? '—'
      : new Intl.NumberFormat(locale, { style: 'percent', maximumFractionDigits: 2 }).format(
          value / 10000,
        );
  const totals = proposalDraftTotals(lines);
  return (
    <section className="proposal-sales-calculation">
      <p className="hint">{translate('proposal.salesCalculationHint')}</p>
      <div className="proposal-table-scroll">
        <table className="proposal-table proposal-sales-table">
          <thead>
            <tr>
              {(
                [
                  'service',
                  'quantity',
                  'purchaseNet',
                  'clientUnitPrice',
                  'markup',
                  'saleNet',
                  'profit',
                  'margin',
                  'lineActions',
                ] as const
              ).map((key) => (
                <th scope="col" key={key}>
                  {translate(`proposal.${key}`)}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {lines.map((line) => {
              const amounts = proposalLineFinancials(line);
              const title = name(line);
              return (
                <tr key={line.id} data-loss={amounts.profit !== undefined && amounts.profit < 0n}>
                  <th scope="row">
                    <button
                      type="button"
                      className="proposal-line-name"
                      onClick={() => onDetails(line.id)}
                    >
                      {title}
                    </button>
                    <small>
                      {line.quote_source
                        ? translate('rfq.source')
                        : translate('proposal.manualService')}
                    </small>
                  </th>
                  <td>
                    {formatFixed(line.quantity_milli, 3)}
                    <small>{translate(`registryValue.unit.${line.unit}`)}</small>
                  </td>
                  <td className="proposal-money">
                    {amounts.purchase ? (
                      <>
                        {formatFixed(amounts.purchase.net, 2)}
                        <small>{line.purchase?.currency}</small>
                      </>
                    ) : (
                      '—'
                    )}
                  </td>
                  <td>
                    <div className="proposal-unit-price">
                      <NumberField
                        label={translate('proposal.priceForService', { service: title })}
                        hideLabel
                        value={line.sale.price_minor}
                        onChange={(price_minor) =>
                          onChange({ ...line, sale: { ...line.sale, price_minor } })
                        }
                      />
                      <span>{line.sale.currency}</span>
                    </div>
                    <small>
                      {translate(
                        line.sale.price_mode === 'gross'
                          ? 'proposal.priceGross'
                          : 'proposal.priceNet',
                      )}
                    </small>
                  </td>
                  <td>
                    <MarkupField
                      line={line}
                      label={translate('proposal.markupForService', { service: title })}
                      onChange={onChange}
                    />
                    <small>
                      {amounts.markup === undefined ? translate('proposal.markupUnavailable') : '%'}
                    </small>
                  </td>
                  <td className="proposal-money">
                    {formatFixed(amounts.sale.net, 2)}
                    <small>{line.sale.currency}</small>
                  </td>
                  <td className="proposal-money proposal-profit-amount">
                    {amounts.profit === undefined ? (
                      <>
                        <span>—</span>
                        <small>
                          {translate(
                            line.purchase
                              ? 'proposal.afterCalculation'
                              : 'proposal.profitNeedsPurchaseShort',
                          )}
                        </small>
                      </>
                    ) : (
                      <>
                        {formatFixed(amounts.profit, 2)}
                        <small>{line.sale.currency}</small>
                      </>
                    )}
                  </td>
                  <td>{percentage(amounts.margin)}</td>
                  <td>
                    <button
                      type="button"
                      className="proposal-line-remove"
                      aria-label={translate('proposal.removeService', { service: title })}
                      onClick={() => onRemove(line.id)}
                    >
                      ×
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
          <tfoot>
            {totals.map((total) => (
              <tr key={total.currency}>
                <th scope="row" colSpan={2}>
                  {translate('proposal.currencyTotal', { currency: total.currency })}
                </th>
                <td className="proposal-money">{formatFixed(total.purchase, 2)}</td>
                <td colSpan={2}></td>
                <td className="proposal-money">{formatFixed(total.sale, 2)}</td>
                <td className="proposal-money">
                  {total.profit === undefined ? '—' : formatFixed(total.profit, 2)}
                </td>
                <td colSpan={2}></td>
              </tr>
            ))}
          </tfoot>
        </table>
      </div>
      <p className="hint">{translate('proposal.draftTotalsHint')}</p>
      {lines.some((line) => {
        const { profit } = proposalLineFinancials(line);
        return profit !== undefined && profit < 0n;
      }) && (
        <p className="error" role="status">
          {translate('proposal.lossHint')}
        </p>
      )}
    </section>
  );
}
