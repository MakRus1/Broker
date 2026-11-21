import { proposalAmounts } from './proposalAmounts';
import type { ProposalLine, ProposalPrice } from './proposals';

function roundedRatio(value: bigint, divisor: bigint): bigint {
  return value < 0n ? -((-value + divisor / 2n) / divisor) : (value + divisor / 2n) / divisor;
}

export function proposalLineFinancials(line: ProposalLine) {
  const sale = proposalAmounts(line.quantity_milli, line.sale);
  const purchase = line.purchase ? proposalAmounts(line.quantity_milli, line.purchase) : undefined;
  const profit =
    purchase && line.purchase?.currency === line.sale.currency
      ? sale.net - purchase.net
      : undefined;
  const markup =
    profit !== undefined && purchase && purchase.net > 0n
      ? Number(roundedRatio(profit * 10000n, purchase.net))
      : undefined;
  const margin =
    profit !== undefined && sale.net > 0n
      ? Number(roundedRatio(profit * 10000n, sale.net))
      : undefined;
  return { sale, purchase, profit, markup, margin };
}

/** Markup is based on net purchase for the full quantity; stored unit price retains sale VAT settings. */
export function salePriceForMarkup(
  line: ProposalLine,
  basisPoints: number,
): ProposalPrice | undefined {
  const { purchase } = proposalLineFinancials(line);
  if (
    !purchase ||
    purchase.net <= 0n ||
    line.quantity_milli <= 0 ||
    line.purchase?.currency !== line.sale.currency ||
    !Number.isSafeInteger(basisPoints) ||
    basisPoints < -10000
  )
    return undefined;
  const net = roundedRatio(purchase.net * (10000n + BigInt(basisPoints)), 10000n);
  const rate = line.sale.vat_mode === 'exempt' ? 0n : BigInt(line.sale.vat_rate_basis_points ?? 0);
  const amount =
    line.sale.price_mode === 'gross' ? roundedRatio(net * (10000n + rate), 10000n) : net;
  const price = roundedRatio(amount * 1000n, BigInt(line.quantity_milli));
  if (price > 100000000n) return undefined;
  return { ...line.sale, price_minor: Number(price) };
}

export function proposalDraftTotals(lines: ProposalLine[]) {
  const totals = new Map<
    string,
    { currency: string; purchase: bigint; sale: bigint; profit?: bigint }
  >();
  for (const line of lines) {
    const { sale, purchase, profit } = proposalLineFinancials(line);
    const currencies = new Set([
      line.sale.currency,
      ...(line.purchase ? [line.purchase.currency] : []),
    ]);
    for (const currency of currencies) {
      const previous = totals.get(currency) ?? { currency, purchase: 0n, sale: 0n, profit: 0n };
      totals.set(currency, {
        currency,
        purchase:
          previous.purchase + (currency === line.purchase?.currency ? (purchase?.net ?? 0n) : 0n),
        sale: previous.sale + (currency === line.sale.currency ? sale.net : 0n),
        profit:
          previous.profit !== undefined && profit !== undefined
            ? previous.profit + profit
            : undefined,
      });
    }
  }
  return [...totals.values()].sort((a, b) => a.currency.localeCompare(b.currency));
}
