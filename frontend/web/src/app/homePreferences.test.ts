import { expect, it } from 'vitest';
import {
  favoriteSection,
  favoriteView,
  restoreFavorites,
  visibleSections,
} from './homePreferences';
import { sectionForCard, sectionRegistry } from './workspaceSections';
import { restoreRecentCards } from '../features/search/recentCards';
it('restores bounded favorites without duplicate or unsafe targets', () => {
  expect(
    restoreFavorites([
      favoriteSection('/shipments'),
      favoriteSection('/shipments'),
      null,
      'https://evil.test',
      'section:<script>',
      favoriteView('contracts', 'abc'),
    ]),
  ).toEqual(['section:/shipments', 'view:contracts:abc']);
  expect(
    restoreFavorites(Array.from({ length: 80 }, (_, i) => `view:contracts:${i}`)),
  ).toHaveLength(60);
});
it('resolves the exact preference registry used by each finance screen', () => {
  expect(sectionRegistry('/receipts')).toBe('receipt-payments');
  expect(sectionRegistry('/disbursements')).toBe('disbursement-payments');
  expect(sectionRegistry('/financial-labels')).toBe('financial_labels');
  expect(sectionRegistry('/shipments')).toBe('shipments');
});
it('does not expose sections or recent cards without their current permissions', () => {
  const path = '/service-charges/11111111-1111-4111-8111-111111111111';
  expect(visibleSections([])).toEqual([]);
  expect(sectionForCard(path, [])).toBeUndefined();
  expect(sectionForCard(path, ['finance.read_service_charges'])?.route).toBe('/service-charges');
  expect(sectionForCard('/service-charges/new', ['finance.read_service_charges'])).toBeUndefined();
});
it('shares safely decoded recent records between home and global search', () => {
  const hit = {
    id: 'id',
    route: '/contracts/11111111-1111-4111-8111-111111111111',
    title: 'Contract',
    detail: '',
  };
  expect(
    restoreRecentCards([
      hit,
      { ...hit, route: 'https://evil.test' },
      { id: 'id', route: '/contracts/new', title: 'Draft' },
      { ...hit, title: 123 },
    ]),
  ).toEqual([hit]);
});
