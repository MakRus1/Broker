import { describe, expect, it } from 'vitest';
import { RegistryPageCache, RegistryPageChanged } from './registryPageCache';

function page(index: number) {
  return {
    items: Array.from({ length: 50 }, (_, i) => ({
      id: String(index * 50 + i),
      payload: 'x'.repeat(1000),
    })),
    next_cursor: index + 1,
  };
}
describe('registry page cache', () => {
  it('bounds payload while preserving 100,000 identities and reload cursors', () => {
    const cache = new RegistryPageCache<ReturnType<typeof page>['items'][number], number>();
    for (let i = 0; i < 2000; i++) {
      cache.append(page(i));
      cache.protect([i * 50]);
      cache.evict();
    }
    expect(cache.rows.length).toBe(100000);
    expect(cache.items.length).toBe(400);
    expect(cache.get(0)).toBeUndefined();
    expect(cache.pages[3].cursor).toBe(3);
    cache.protect([150]);
    cache.restore(3, page(3));
    cache.evict();
    expect(cache.get(150)?.id).toBe('150');
    expect(cache.items.length).toBe(400);
  });
  it('rejects changed page order and cursors instead of mixing snapshots', () => {
    const cache = new RegistryPageCache<{ id: string }, number>();
    cache.append(page(0));
    expect(() => cache.restore(0, { ...page(0), items: [...page(0).items].reverse() })).toThrow(
      RegistryPageChanged,
    );
    expect(() => cache.restore(0, { ...page(0), next_cursor: 20 })).toThrow(RegistryPageChanged);
    expect(cache.get(0)?.id).toBe('0');
    expect(() => cache.append(page(0))).toThrow(RegistryPageChanged);
    expect(() => cache.append({ items: [], next_cursor: 1 })).toThrow(RegistryPageChanged);
  });
  it('protects the viewport and releases it when the visible range moves', () => {
    const cache = new RegistryPageCache<{ id: string }, number>(2);
    cache.append(page(0));
    cache.append(page(1));
    cache.append(page(2));
    cache.protect([0]);
    cache.evict();
    expect(cache.get(0)).toBeDefined();
    cache.append(page(3));
    cache.protect([150]);
    cache.evict();
    expect(cache.get(0)).toBeUndefined();
  });
  it('locates rows in variable-size pages', () => {
    const cache = new RegistryPageCache<{ id: string }, number>();
    cache.append({ items: [{ id: 'a' }], next_cursor: 1 });
    cache.append({ items: [{ id: 'b' }, { id: 'c' }] });
    expect([cache.pageAt(0), cache.pageAt(1), cache.pageAt(2)]).toEqual([0, 1, 1]);
    expect(cache.more).toBe(false);
  });
});

it('resolves selection across evicted pages without growing viewport payload', async () => {
  const cache = new RegistryPageCache<ReturnType<typeof page>['items'][number], number>(2);
  for (let i = 0; i < 30; i++) {
    cache.append(page(i));
    cache.evict();
  }
  const calls: number[] = [];
  const result = await cache.readRows(
    new Set(['0', '500', '1499']),
    async (cursor) => {
      calls.push(cursor ?? 0);
      return page(cursor ?? 0);
    },
    new AbortController().signal,
  );
  expect(result.map((row) => row.id)).toEqual(['0', '500', '1499']);
  expect(calls).toEqual([0, 10]);
  expect(cache.items.length).toBe(100);
});

it('rejects a changed or aborted selection instead of returning partial objects', async () => {
  const cache = new RegistryPageCache<{ id: string }, number>(1);
  cache.append(page(0));
  cache.append(page(1));
  cache.evict();
  await expect(
    cache.readRows(new Set(['0', 'missing']), async () => page(0), new AbortController().signal),
  ).rejects.toThrow(RegistryPageChanged);
  await expect(
    cache.readRows(
      new Set(['0']),
      async () => ({ ...page(0), items: page(0).items.slice(1) }),
      new AbortController().signal,
    ),
  ).rejects.toThrow(RegistryPageChanged);
  const controller = new AbortController();
  await expect(
    cache.readRows(
      new Set(['0']),
      async () => {
        controller.abort();
        return page(0);
      },
      controller.signal,
    ),
  ).rejects.toThrow('aborted');
  expect(cache.items.length).toBe(50);
});
