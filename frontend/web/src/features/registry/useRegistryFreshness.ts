import { useRef, useSyncExternalStore } from 'react';
import {
  registryActivitySnapshot,
  registryDataRevision,
  subscribeRegistryActivity,
} from '../../shared/api/dataInvalidation';
import { RegistryFreshness } from './registryFreshness';

export function useRegistryFreshness(identity: readonly unknown[]) {
  const version = useSyncExternalStore(
    subscribeRegistryActivity,
    registryActivitySnapshot,
    () => 0,
  );
  const freshness = useRef(new RegistryFreshness());
  const revision = registryDataRevision();
  return {
    version,
    clear: () => freshness.current.clear(),
    isFresh: () => freshness.current.isFresh(identity, revision),
    complete: () => freshness.current.complete(identity, revision),
  };
}
