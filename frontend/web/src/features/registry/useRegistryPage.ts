import { useCallback, useEffect, useRef, useState } from 'react';

interface Page<T, Cursor> {
  items: T[];
  next_cursor?: Cursor | null;
}

/** Owns pagination and discards responses from a previous query or inactive registry. */
export function useRegistryPage<T, Cursor>({
  active,
  refreshVersion,
  loadPage,
  onError,
}: {
  active: boolean;
  refreshVersion: number;
  loadPage: (cursor: Cursor | undefined, signal: AbortSignal) => Promise<Page<T, Cursor>>;
  onError: (error: unknown) => void;
}) {
  const [items, setItems] = useState<T[]>([]);
  const [loading, setLoading] = useState(false);
  const [failed, setFailed] = useState(false);
  const [more, setMore] = useState(false);
  const cursor = useRef<Cursor | undefined>(undefined);
  const pending = useRef<AbortController | null>(null);
  const load = useCallback(
    async (append = false) => {
      if (!active || (append && (pending.current || cursor.current === undefined))) return;
      pending.current?.abort();
      const controller = new AbortController();
      pending.current = controller;
      if (!append) {
        cursor.current = undefined;
        setMore(false);
      }
      setLoading(true);
      setFailed(false);
      try {
        const page = await loadPage(append ? cursor.current : undefined, controller.signal);
        if (controller.signal.aborted) return;
        setItems((old) => (append ? [...old, ...page.items] : page.items));
        cursor.current = page.next_cursor ?? undefined;
        setMore(cursor.current !== undefined);
      } catch (error) {
        if (!controller.signal.aborted) {
          setFailed(true);
          onError(error);
        }
      } finally {
        if (pending.current === controller) {
          pending.current = null;
          setLoading(false);
        }
      }
    },
    [active, loadPage, onError],
  );
  useEffect(() => {
    void load();
    return () => {
      pending.current?.abort();
      pending.current = null;
    };
  }, [load, refreshVersion]);
  return { items, loading, failed, more, refresh: () => load(), loadMore: () => load(true) };
}
