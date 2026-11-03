import { useEffect, useRef, useState } from 'react';
import type { Session } from '../api/session';

const lifetime = 7 * 24 * 60 * 60 * 1000;
export type DraftEnvelope = { version: 1; revision: number; savedAt: number; value: unknown };
export function draftStorageKey(
  session: Pick<Session, 'actor' | 'organization_id'>,
  form: string,
  id: string,
) {
  return `broker:draft:v1:${encodeURIComponent(session.actor)}:${encodeURIComponent(session.organization_id)}:${form}:${id}`;
}
export function parseDraft(raw: string | null, now = Date.now()): DraftEnvelope | null {
  try {
    const value = JSON.parse(raw ?? 'null');
    return value?.version === 1 &&
      Number.isSafeInteger(value.revision) &&
      value.revision >= 0 &&
      typeof value.savedAt === 'number' &&
      value.savedAt <= now &&
      now - value.savedAt < lifetime &&
      value.value &&
      typeof value.value === 'object' &&
      !Array.isArray(value.value)
      ? value
      : null;
  } catch {
    return null;
  }
}
export function useDraftRecovery<T>({
  storageKey,
  revision,
  value,
  dirty,
  enabled,
  onRestore,
  revisionForDraft,
}: {
  storageKey: string;
  revision: number;
  value: T;
  dirty: boolean;
  enabled: boolean;
  onRestore: (raw: unknown) => void;
  revisionForDraft?: (raw: unknown) => number;
}) {
  const [candidate, setCandidate] = useState<DraftEnvelope | null>(null);
  const [failed, setFailed] = useState(false);
  const [saved, setSaved] = useState(false);
  const [persisted, setPersisted] = useState(false);
  const [loadedKey, setLoadedKey] = useState('');
  const suppressed = useRef<string | undefined>(undefined);
  const current = useRef({ value, dirty, revision });
  current.current = { value, dirty, revision };
  useEffect(() => {
    setLoadedKey('');
    setSaved(false);
    setPersisted(false);
    suppressed.current = undefined;
    try {
      const raw = localStorage.getItem(storageKey);
      const draft = parseDraft(raw);
      setCandidate(draft);
      if (raw && !draft) localStorage.removeItem(storageKey);
    } catch {
      setFailed(true);
    }
    setLoadedKey(storageKey);
  }, [storageKey]);
  const serialized = JSON.stringify(value);
  useEffect(() => {
    if (loadedKey === storageKey && enabled && !candidate && !dirty && persisted) {
      try {
        localStorage.removeItem(storageKey);
        setPersisted(false);
      } catch {
        setFailed(true);
      }
      return;
    }
    if (
      loadedKey !== storageKey ||
      !enabled ||
      candidate ||
      !dirty ||
      suppressed.current === serialized
    )
      return;
    setSaved(false);
    try {
      localStorage.setItem(
        storageKey,
        JSON.stringify({ version: 1, revision, savedAt: Date.now(), value }),
      );
      setFailed(false);
      setPersisted(true);
    } catch {
      setFailed(true);
    }
  }, [storageKey, loadedKey, enabled, candidate, dirty, revision, serialized, persisted]);
  useEffect(() => {
    if (!saved) return;
    const timer = window.setTimeout(() => setSaved(false), 4000);
    return () => window.clearTimeout(timer);
  }, [saved]);
  function clear() {
    suppressed.current = JSON.stringify(current.current.value);
    try {
      localStorage.removeItem(storageKey);
    } catch {
      setFailed(true);
    }
    setCandidate(null);
    setPersisted(false);
  }
  const expectedRevision =
    candidate && revisionForDraft ? revisionForDraft(candidate.value) : revision;
  return {
    candidate,
    stale: !!candidate && candidate.revision !== expectedRevision,
    failed,
    saved,
    persisted,
    discard: clear,
    markSaved() {
      clear();
      setSaved(true);
    },
    restore() {
      if (!candidate || candidate.revision !== expectedRevision) return;
      try {
        onRestore(candidate.value);
        setPersisted(true);
        setCandidate(null);
      } catch {
        setFailed(true);
      }
    },
  };
}
