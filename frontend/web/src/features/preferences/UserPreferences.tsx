import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
  useSyncExternalStore,
  type ReactNode,
} from 'react';
import { request, ApiError } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { PreferenceStore, type Preference, type PreferenceTransport } from './preferenceStore';

const PreferencesContext = createContext<PreferenceStore | undefined>(undefined);
const SessionContext = createContext<{
  session: Session | null | undefined;
  setSession: (session: Session | null) => void;
}>({ session: undefined, setSession: () => {} });
export const usePreferences = () => useContext(PreferencesContext);
export function usePreferencesSession() {
  const { session, setSession } = useContext(SessionContext);
  return [session, setSession] as const;
}
function PreferenceNotice({ store }: { store: PreferenceStore }) {
  useSyncExternalStore(store.subscribe, store.snapshot);
  return store.error ? (
    <div className="preference-notice" role="status">
      {store.error}
      <button onClick={() => void store.retry()}>Повторить</button>
    </div>
  ) : null;
}
export function UserPreferences({ children }: { children: ReactNode }) {
  const [session, setSession] = useState<Session | null>();
  const [store, setStore] = useState<PreferenceStore>();
  const [error, setError] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    let current: PreferenceStore | undefined;
    void (async () => {
      try {
        const user = await request<Session>(
          '/api/session',
          undefined,
          undefined,
          controller.signal,
        );
        const transport: PreferenceTransport = {
          list: async () =>
            (
              await request<{ preferences: Preference[] }>(
                '/api/preferences/list',
                user,
                { organization_id: user.organization_id },
                controller.signal,
              )
            ).preferences,
          save: async (key, value, expected_revision) =>
            (
              await request<{ preference: Preference }>(
                '/api/preferences/save',
                user,
                { key, value, expected_revision, organization_id: user.organization_id },
                controller.signal,
              )
            ).preference,
        };
        let entries: Preference[] = [];
        let ready = true;
        try {
          entries = await transport.list();
        } catch {
          ready = false;
        }
        if (controller.signal.aborted) return;
        let storage: Storage | undefined;
        try {
          storage = localStorage;
        } catch {
          /* Браузер может запретить доступ. */
        }
        current = new PreferenceStore(
          user.actor,
          user.subject,
          user.organization_id,
          transport,
          entries,
          storage,
          ready,
        );
        setStore(current);
        setSession(user);
      } catch (value) {
        if (!controller.signal.aborted) {
          setSession(null);
          if (!(value instanceof ApiError && value.status === 401))
            setError(value instanceof Error ? value.message : 'Не удалось открыть сессию');
        }
      }
    })();
    return () => {
      controller.abort();
      current?.dispose();
    };
  }, []);
  const changeSession = useCallback(
    (next: Session | null) => {
      if (!next) {
        store?.dispose();
        setStore(undefined);
      }
      setSession(next);
    },
    [store],
  );
  return (
    <SessionContext.Provider value={{ session, setSession: changeSession }}>
      <PreferencesContext.Provider value={store}>
        {session && store && <PreferenceNotice store={store} />}
        {error && <div role="alert">{error}</div>}
        {children}
      </PreferencesContext.Provider>
    </SessionContext.Provider>
  );
}
