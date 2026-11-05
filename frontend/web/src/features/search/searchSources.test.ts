import { beforeEach, expect, it, vi } from 'vitest';
import { permittedSources, searchRequest, searchSources, sourceForRoute } from './searchSources';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
vi.mock('../../shared/api/http', () => ({ request: vi.fn() }));
const session: Session = {
  actor: 'actor',
  subject: 'user',
  organization_id: 'org',
  display_name: 'User',
  csrf_token: 'csrf',
  permissions: [],
};
beforeEach(() => vi.clearAllMocks());
it('queries only sources covered by the current permissions', () => {
  expect(permittedSources(session)).toEqual([]);
  expect(
    permittedSources({ ...session, permissions: ['shipments.read', 'finance.read_invoices'] }).map(
      (s) => s.id,
    ),
  ).toEqual(['shipments', 'outgoing-invoices', 'incoming-invoices']);
});
it('does not allow recent records outside the configured card routes', () => {
  const sources = permittedSources({ ...session, permissions: ['shipments.read'] });
  const id = '11111111-1111-4111-8111-111111111111';
  expect(sourceForRoute('/shipments/' + id + '/edit', sources)?.id).toBe('shipments');
  for (const path of [
    '/shipments/new',
    '/shipments',
    'https://example.com',
    '/counterparties/' + id,
  ])
    expect(sourceForRoute(path, sources)).toBeUndefined();
});
it('sends a bounded contains query through the existing BFF with cancellation and organization session', async () => {
  vi.mocked(request).mockResolvedValue({
    items: [{ id: 'id', number: 'S-12', shipment_date: '2026-11-05', cargo_description: 'Cargo' }],
  });
  const signal = new AbortController().signal;
  const hits = await searchSources[0].load(session, 'S-1', signal);
  expect(request).toHaveBeenCalledExactlyOnceWith(
    '/api/v1/shipments/list',
    session,
    {
      sort: 'shipment_date',
      descending: true,
      filters: [{ field: 'number', query: 'S-1' }],
      limit: 5,
    },
    signal,
  );
  expect(hits).toEqual([
    { id: 'id', title: 'S-12', detail: '05.11.2026 · Cargo', route: '/shipments/id' },
  ]);
});
it('keeps incoming and outgoing invoice searches separate', async () => {
  vi.mocked(request).mockResolvedValue({ items: [] });
  for (const direction of ['incoming', 'outgoing']) {
    const source = searchSources.find((s) => s.id === direction + '-invoices')!;
    await source.load(session, '123', new AbortController().signal);
    expect(vi.mocked(request).mock.lastCall?.[2]).toMatchObject({
      direction,
      limit: 5,
      filters: [
        { field: 'number', query: '123' },
        { field: 'archived', values: ['false'] },
      ],
    });
  }
});
it('uses the same query for show all without a preview limit', () => {
  const source = searchSources.find((s) => s.id === 'transportations')!;
  expect(searchRequest(source, 'ABCD')).toEqual({
    sort: 'container_number',
    descending: false,
    filters: [{ field: 'container_number', query: 'ABCD' }],
  });
});
