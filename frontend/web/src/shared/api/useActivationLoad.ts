import { useEffect, useRef } from 'react';

/** Reuse a successful load during quick switches between open workspace tabs.
 * A later activation refreshes it; failures and cancelled loads are never fresh.
 */
export function useActivationLoad(
  active: boolean,
  load: (signal: AbortSignal) => Promise<void>,
  onError: (error: unknown) => void,
) {
  const completed = useRef<{ load: typeof load; at: number } | undefined>(undefined);
  const errorCallback = useRef(onError);
  errorCallback.current = onError;
  useEffect(() => {
    if (!active) return;
    if (completed.current?.load === load && Date.now() - completed.current.at < 30_000) return;
    const controller = new AbortController();
    void load(controller.signal)
      .then(() => {
        if (!controller.signal.aborted) completed.current = { load, at: Date.now() };
      })
      .catch((error) => {
        if (!controller.signal.aborted) errorCallback.current(error);
      });
    return () => controller.abort();
  }, [active, load]);
}
