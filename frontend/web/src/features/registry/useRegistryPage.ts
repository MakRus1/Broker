import { useRegistryFreshness } from './useRegistryFreshness';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  RegistryPageCache,
  RegistryPageChanged,
  type RegistryPage,
  type RegistryPageSource,
} from './registryPageCache';

export function useRegistryPage<T extends { id: string }, Cursor>({
  active,
  refreshVersion,
  loadPage,
  onError,
}: {
  active: boolean;
  refreshVersion: number;
  loadPage: (cursor: Cursor | undefined, signal: AbortSignal) => Promise<RegistryPage<T, Cursor>>;
  onError: (error: unknown) => void;
}) {
  const freshness = useRegistryFreshness([loadPage, refreshVersion]);
  const store = useRef(new RegistryPageCache<T, Cursor>());
  const [renderVersion, render] = useState(0);
  const [loading, setLoading] = useState(active);
  const [failed, setFailed] = useState(false);
  const pending = useRef(new Map<number, AbortController>());
  const failures = useRef(new Set<number>());
  const latestError = useRef(onError);
  latestError.current = onError;
  const revision = useRef(0);
  const refreshing = useRef(false);
  const notify = () => render((old) => old + 1);
  const abort = () => {
    for (const request of pending.current.values()) request.abort();
    pending.current.clear();
  };
  const load = useCallback(
    async (index: number, reset = false) => {
      if (!active || (!reset && (refreshing.current || pending.current.has(index)))) return;
      if (!reset && index === store.current.pages.length && !store.current.more) return;
      if (reset) {
        revision.current++;
        abort();
        failures.current.clear();
        freshness.clear();
        refreshing.current = true;
      }
      const cache = reset ? new RegistryPageCache<T, Cursor>() : store.current;
      const generation = revision.current;
      const controller = new AbortController();
      pending.current.set(index, controller);
      setLoading(true);
      setFailed(false);
      try {
        const page = await loadPage(
          index === cache.pages.length ? cache.next : cache.pages[index]?.cursor,
          controller.signal,
        );
        if (controller.signal.aborted || generation !== revision.current) return;
        if (index === cache.pages.length) cache.append(page);
        else cache.restore(index, page);
        if (reset) {
          store.current = cache;
          freshness.complete();
        }
        failures.current.delete(index);
        cache.evict();
        notify();
      } catch (error) {
        if (!controller.signal.aborted && generation === revision.current) {
          failures.current.add(index);
          setFailed(true);
          latestError.current(error);
          notify();
          // Never merge a page from a changed keyset into old positions.
          if (error instanceof RegistryPageChanged) freshness.clear();
        }
      } finally {
        if (pending.current.get(index) === controller) {
          pending.current.delete(index);
          if (reset) refreshing.current = false;
          setLoading(pending.current.size > 0);
        }
      }
    },
    [active, loadPage, refreshVersion, freshness.version],
  );
  useEffect(() => {
    if (active && !freshness.isFresh()) void load(0, true);
    return () => {
      revision.current++;
      abort();
      refreshing.current = false;
      setLoading(false);
    };
  }, [load, freshness.version]);
  const ensure = useCallback(
    (indices: readonly number[]) => {
      if (refreshing.current) return;
      const cache = store.current;
      cache.protect(indices);
      const before = cache.items.length;
      cache.evict();
      if (before !== cache.items.length) notify();
      // At most two page requests, nearest visible pages before prefetch neighbours.
      const desired = [
        ...new Set(indices.map((index) => cache.pageAt(index))),
        ...cache.protectedPages,
      ];
      for (const index of desired) {
        if (pending.current.size >= 2) break;
        if (cache.pages[index] && !cache.pages[index].items && !failures.current.has(index))
          void load(index);
      }
    },
    [load],
  );
  const cache = store.current;
  const readRows = useCallback(
    async (ids: ReadonlySet<string>, signal: AbortSignal) => {
      const generation = revision.current;
      const result = await store.current.readRows(
        ids,
        (cursor) => loadPage(cursor, signal),
        signal,
      );
      if (generation !== revision.current)
        throw new RegistryPageChanged('Registry selection changed');
      return result;
    },
    [loadPage, refreshVersion, active, cache],
  );
  const items = useMemo(() => cache.items, [cache, renderVersion]);
  const ids = useMemo(() => cache.rows.map((row) => row.id), [cache.rows]);
  const source: RegistryPageSource<T> = {
    rows: cache.rows,
    get: (index) => cache.get(index),
    ensure,
    failed: (index) => failures.current.has(cache.pageAt(index)),
    retry: (index) => {
      const page = cache.pageAt(index);
      failures.current.delete(page);
      if (!freshness.isFresh()) void load(0, true);
      else void load(page);
    },
  };
  return {
    items,
    ids,
    readRows,
    generation: revision.current,
    count: cache.rows.length,
    source,
    loading,
    failed,
    more: cache.more && !refreshing.current,
    refresh: () => load(0, true),
    loadMore: () =>
      failures.current.size && !freshness.isFresh() ? load(0, true) : load(cache.pages.length),
  };
}
