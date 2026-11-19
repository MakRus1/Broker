import type { ProposalPrice } from './proposals';

export function proposalAmounts(quantity: number, price: ProposalPrice) {
  const amount = (BigInt(quantity) * BigInt(price.price_minor) + 500n) / 1000n;
  const rate = price.vat_mode === 'exempt' ? 0n : BigInt(price.vat_rate_basis_points ?? 0);
  const divisor = price.price_mode === 'gross' ? 10000n + rate : 10000n;
  const vat = (amount * rate + divisor / 2n) / divisor;
  const net = price.price_mode === 'gross' ? amount - vat : amount;
  return { net, vat, total: net + vat };
}
