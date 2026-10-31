import { useCallback, useEffect, useMemo, useState } from 'react';
import { request, type Session } from '../api/api';
import { emptyRegistry, RegistryLoader, type RegistryPage } from './registryLoader';
export interface RegistryQuery {
  sort: string;
  descending: boolean;
  query?: string;
  enabled?: boolean;
  role?: string;
  country_code?: string;
}
export function useRegistry<T>(path: string, session: Session, query: object, enabled = true) {
  const key = JSON.stringify(query);
  const [state, setState] = useState(emptyRegistry<T>);
  const [reload, setReload] = useState(0);
  const loader = useMemo(
    () =>
      new RegistryLoader<T>(
        (body, signal) => request<RegistryPage<T>>(path, session, body, signal),
        setState,
      ),
    [path, session],
  );
  useEffect(() => {
    if (enabled) void loader.start(JSON.parse(key));
    else loader.reset();
    return () => loader.stop();
  }, [loader, key, enabled, reload]);
  const loadMore = useCallback(() => loader.more(), [loader]);
  const refresh = useCallback(() => setReload((value) => value + 1), []);
  return { ...state, hasMore: Boolean(state.next_cursor), loadMore, refresh };
}
