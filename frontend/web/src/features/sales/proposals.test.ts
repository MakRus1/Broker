import { describe, it, expect, vi } from 'vitest';
import { copyProposal, proposalMargin, type Proposal } from './proposals';
describe('proposal calculations and copies', () => {
  it('does not invent a margin for a zero sale', () => {
    expect(
      proposalMargin({
        currency: 'RUB',
        sale: { net_minor: 0, vat_minor: 0, total_minor: 0 },
        purchase: { net_minor: 100, vat_minor: 0, total_minor: 100 },
      }),
    ).toBe('—');
  });
  it('uses the net sale as the margin denominator', () => {
    expect(
      proposalMargin({
        currency: 'USD',
        sale: { net_minor: 230000, vat_minor: 0, total_minor: 230000 },
        purchase: { net_minor: 200000, vat_minor: 0, total_minor: 200000 },
      }),
    ).toBe('13.0%');
  });
  it('copies input into an independent variant without financial results or status', () => {
    vi.spyOn(crypto, 'randomUUID').mockReturnValue('11111111-1111-4111-8111-111111111111');
    const value = {
      input: {
        id: 'original',
        inquiry_id: 'inquiry',
        title: 'Sea',
        calculation_date: '2026-11-08',
        lines: [{ id: 'line', sale: { price_minor: 230000 } }],
      },
      status: 'accepted',
      revision: 3,
    } as Proposal;
    const copy = copyProposal(value);
    copy.lines[0].sale.price_minor = 100;
    expect(value.input.lines[0].sale.price_minor).toBe(230000);
    expect(copy.id).not.toBe(value.input.id);
    expect(copy).not.toHaveProperty('status');
    expect(copy).not.toHaveProperty('revision');
    vi.restoreAllMocks();
  });
});
