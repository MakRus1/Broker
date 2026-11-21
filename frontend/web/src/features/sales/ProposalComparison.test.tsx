import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { ProposalComparison } from './ProposalComparison';
import type { Proposal } from './proposals';

const amount = { net_minor: 10000, vat_minor: 0, total_minor: 10000 };
const line: Proposal['lines'][number] = {
  input: {
    id: 'line',
    label_id: 'freight',
    unit: 'transportation',
    quantity_milli: 1000,
    sale: { price_minor: 10000, currency: 'RUB', price_mode: 'net', vat_mode: 'exempt' },
  },
  label_name: 'Freight',
  sale: amount,
};
function proposal(id: string, lines = [line]): Proposal {
  const total = { currency: 'RUB', sale: amount, purchase: { ...amount, net_minor: 0 } };
  return {
    input: {
      id,
      inquiry_id: 'inquiry',
      title: id,
      calculation_date: '2026-11-21',
      lines: lines.map((value) => value.input),
    },
    lines,
    totals: [total],
    reporting: total,
    customer_name: 'Customer',
    inquiry_revision: 1,
    revision: 1,
    status: 'draft',
    reporting_currency: 'RUB',
    rates: [],
  };
}
const inquiry = {
  id: 'inquiry',
  organization_id: 'org',
  customer_id: 'customer',
  manager_subject: 'author',
  status: 'draft' as const,
  number: 'ENQ-1',
  revision: 1,
  converting: false,
  route_segments: [],
};
it('marks missing services and keeps different quantities in separate rows', () => {
  const html = renderToStaticMarkup(
    <ProposalComparison
      inquiry={inquiry}
      items={[
        proposal('A'),
        proposal('B', [{ ...line, input: { ...line.input, quantity_milli: 2000 } }]),
      ]}
      onOpen={() => {}}
    />,
  );
  expect(html.match(/Не включено/g)).toHaveLength(2);
  expect(html.match(/Есть различия/g)).toHaveLength(2);
  expect(html).toContain('1,000');
  expect(html).toContain('2,000');
});
it('does not present missing purchase as profit and identifies the accepted proposal', () => {
  const html = renderToStaticMarkup(
    <ProposalComparison
      inquiry={{ ...inquiry, accepted_proposal_id: 'A' }}
      items={[proposal('A')]}
      onOpen={() => {}}
    />,
  );
  expect(html).toContain('Закупка указана не для всех услуг');
  expect(html).toContain('Выбран');
  const profit = html.slice(html.indexOf('Прибыль'), html.indexOf('Маржа'));
  expect(profit).toContain('—');
  expect(profit).not.toContain('100,00');
});
it('does not mark identical services as different despite separate line identities', () => {
  const html = renderToStaticMarkup(
    <ProposalComparison
      inquiry={inquiry}
      items={[proposal('A'), proposal('B', [{ ...line, input: { ...line.input, id: 'other' } }])]}
      onOpen={() => {}}
    />,
  );
  expect(html).not.toContain('Есть различия');
});
