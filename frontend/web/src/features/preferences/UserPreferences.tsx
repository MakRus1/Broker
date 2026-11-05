import { createContext, useCallback, useContext, useEffect, useState, type ReactNode } from 'react';
import { interfaceText } from '../../shared/localization/interfaceText';
import { request, ApiError } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { PreferenceStore, type Preference, type PreferenceTransport } from './preferenceStore';

export interface RoleRegistryDefault {
  role: string;
  registry: string;
  value: { columns: unknown; query: unknown };
  revision: number;
}
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
        let roleDefaults: RoleRegistryDefault[] = [];
        const transport: PreferenceTransport = {
          list: async () => {
            const result = await request<{
              preferences: Preference[];
              role_defaults?: RoleRegistryDefault[];
            }>(
              '/api/preferences/list',
              user,
              { organization_id: user.organization_id },
              controller.signal,
            );
            roleDefaults = result.role_defaults ?? [];
            return result.preferences;
          },
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
          Object.fromEntries(
            roleDefaults.flatMap((item) => [
              [`${item.registry}:columns:v1`, item.value.columns],
              [`${item.registry}:query:v1`, item.value.query],
            ]),
          ),
        );
        setStore(current);
        setSession(user);
      } catch (value) {
        if (!controller.signal.aborted) {
          setSession(null);
          if (!(value instanceof ApiError && value.status === 401))
            setError(value instanceof Error ? value.message : interfaceText('errors.session'));
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
        {error && <div role="alert">{error}</div>}
        {children}
      </PreferencesContext.Provider>
    </SessionContext.Provider>
  );
}
