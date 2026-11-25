import { useEffect, useState } from 'react';
import { request } from './http';
import type { Session } from './session';

type ContainerGroup = { code: string; name: string; iso_codes: string[] };

export function useContainerTypes(session: Session, enabled = true) {
  const [types, setTypes] = useState<string[]>([]);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    setTypes([]);
    setFailure('');
    if (!enabled) return;
    const controller = new AbortController();
    request<{ items: ContainerGroup[] }>(
      '/api/v1/container-catalog',
      session,
      undefined,
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted) setTypes(items.map(({ code }) => code));
      })
      .catch((error: Error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [session, enabled]);
  return { types, failure };
}
