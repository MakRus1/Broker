import { expect, it } from 'vitest';
import {
  proposalDraftTotals,
  proposalLineFinancials,
  salePriceForMarkup,
} from './proposalSalesCalculation';
import type { ProposalLine } from './proposals';
const line: ProposalLine = {
  id: 'freight',
  label_id: 'freight',
  unit: 'container',
  quantity_milli: 2000,
  purchase: { price_minor: 800000, currency: 'RUB', price_mode: 'net', vat_mode: 'exempt' },
  sale: {
    price_minor: 1220000,
    currency: 'RUB',
    price_mode: 'gross',
    vat_mode: 'rate',
    vat_rate_basis_points: 2200,
  },
};
it('calculates markup and margin on net amounts for the full quantity', () => {
  const result = proposalLineFinancials(line);
  expect(result.purchase?.net).toBe(1600000n);
  expect(result.sale.net).toBe(2000000n);
  expect(result.profit).toBe(400000n);
  expect(result.markup).toBe(2500);
  expect(result.margin).toBe(2000);
  expect(salePriceForMarkup(line, 2500)).toEqual(line.sale);
  expect(line.sale.price_minor).toBe(1220000);
});
it('applies zero and negative markups while retaining currency and VAT settings', () => {
  const zero = salePriceForMarkup(line, 0)!;
  expect(zero.price_minor).toBe(976000);
  expect(proposalLineFinancials({ ...line, sale: zero }).profit).toBe(0n);
  const loss = salePriceForMarkup(line, -2500)!;
  expect(loss.price_minor).toBe(732000);
  expect(proposalLineFinancials({ ...line, sale: loss }).markup).toBe(-2500);
  expect(salePriceForMarkup(line, -10000)?.price_minor).toBe(0);
  expect(salePriceForMarkup(line, -10001)).toBeUndefined();
});
it('handles fractional quantities and rounds the stored unit price to cents', () => {
  const fractional = {
    ...line,
    quantity_milli: 1500,
    purchase: { ...line.purchase!, price_minor: 101 },
    sale: { ...line.sale, vat_mode: 'exempt' as const, price_mode: 'net' as const },
  };
  const sale = salePriceForMarkup(fractional, 2000)!;
  expect(sale.price_minor).toBe(121);
  expect(proposalLineFinancials({ ...fractional, sale }).sale.net).toBe(182n);
});
it('does not invent a markup or profit for missing, zero or incompatible purchases', () => {
  for (const value of [
    { ...line, purchase: undefined },
    { ...line, purchase: { ...line.purchase!, price_minor: 0 } },
    { ...line, quantity_milli: 0 },
    { ...line, sale: { ...line.sale, currency: 'USD' } },
  ])
    expect(salePriceForMarkup(value, 2500)).toBeUndefined();
  expect(proposalLineFinancials({ ...line, purchase: undefined }).profit).toBeUndefined();
  expect(salePriceForMarkup(line, 10000000)).toBeUndefined();
});
it('keeps currencies separate and does not present a partial profit as a complete total', () => {
  const totals = proposalDraftTotals([
    line,
    { ...line, id: 'usd', sale: { ...line.sale, currency: 'USD' } },
  ]);
  expect(totals).toEqual([
    { currency: 'RUB', purchase: 3200000n, sale: 2000000n, profit: undefined },
    { currency: 'USD', purchase: 0n, sale: 2000000n, profit: undefined },
  ]);
  expect(proposalDraftTotals([line])[0].profit).toBe(400000n);
});
