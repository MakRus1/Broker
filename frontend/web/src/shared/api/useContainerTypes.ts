import { useEffect, useState } from 'react';
import { request } from './http';
import type { Session } from './session';

type ContainerGroup = { code: string; name: string; iso_codes: string[] };

export function useContainerTypes(session: Session, enabled = true) {
  const [groups, setGroups] = useState<ContainerGroup[]>([]);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    setGroups([]);
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
        if (!controller.signal.aborted) setGroups(items);
      })
      .catch((error: Error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [session, enabled]);
  return { groups, types: groups.map(({ code }) => code), failure };
}
