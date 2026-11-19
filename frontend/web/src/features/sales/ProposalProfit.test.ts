import { createElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { ProposalProfit } from './ProposalProfit';
import type { ProposalLine } from './proposals';

const line: ProposalLine = {
  id: 'line',
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
const render = (value: ProposalLine) =>
  renderToStaticMarkup(createElement(ProposalProfit, { line: value }));
it('compares net amounts for the full quantity and distinguishes markup from margin', () => {
  const html = render(line);
  expect(html).toContain('4 000,00');
  expect(html).toContain('25 %');
  expect(html).toContain('20 %');
  expect(html).not.toContain('role="status"');
});
it('shows zero profit and loss without blocking the form, including a zero purchase', () => {
  expect(render({ ...line, sale: { ...line.purchase! } })).toContain('Прибыль равна нулю');
  expect(render({ ...line, sale: { ...line.purchase!, price_minor: 700000 } })).toContain(
    'услуга убыточна',
  );
  expect(render({ ...line, purchase: { ...line.purchase!, price_minor: 0 } })).not.toContain(
    'Infinity',
  );
});
it('does not claim a profit without purchase or between different currencies', () => {
  expect(render({ ...line, purchase: undefined })).toContain('Добавьте закупку');
  const html = render({ ...line, sale: { ...line.sale, currency: 'USD' } });
  expect(html).toContain('Валюты закупки и продажи отличаются');
  expect(html).not.toContain('<dt>Прибыль</dt>');
});
