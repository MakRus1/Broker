import { request } from '../../shared/api/http';
import {
  createContext,
  useContext,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import {
  clearPublishedMessages,
  replacePublishedMessages,
  validatePublishedMessages,
  initialLanguagePreference,
  resolveLocale,
  restoreLanguage,
  translate,
  type Locale,
  type MessageKey,
} from '../../../../shared/i18n/catalog';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { usePreferencesSession } from '../preferences/UserPreferences';

const Context = createContext({
  locale: 'ru' as Locale,
  catalogVersion: 0,
  translate: (key: MessageKey, values?: Record<string, string | number>) =>
    translate('ru', key, values),
  preference: 'auto',
  setPreference: (_value: string) => {},
});
export const useLocalization = () => useContext(Context);
export function Localization({ children }: { children: ReactNode }) {
  const [session] = usePreferencesSession();
  const [devicePreference, setDevicePreference] = useState(initialLanguagePreference);
  const [profilePreference, setProfilePreference] = useRegistryPreference(
    preferenceKey(session?.subject ?? '', 'appearance', 'language'),
    devicePreference,
    restoreLanguage,
  );
  const preference = session ? profilePreference : devicePreference;
  function setPreference(value: string) {
    const next = restoreLanguage(value);
    setDevicePreference(next);
    if (session) setProfilePreference(next);
  }
  const [languages, setLanguages] = useState(() => navigator.languages);
  useEffect(() => {
    const changed = () => setLanguages(navigator.languages);
    window.addEventListener('languagechange', changed);
    return () => window.removeEventListener('languagechange', changed);
  }, []);
  const locale = resolveLocale(preference, languages);
  const sessionRef = useRef(session);
  sessionRef.current = session;
  const [catalogVersion, setCatalogVersion] = useState(0);
  useLayoutEffect(() => {
    clearPublishedMessages();
    setCatalogVersion((version) => version + 1);
    const controller = new AbortController();
    let pending = false;
    async function refresh() {
      if (pending || document.visibilityState === 'hidden') return;
      pending = true;
      try {
        const result = session
          ? await request<{ messages: unknown }>(
              '/api/localization/catalog',
              sessionRef.current!,
              { organization_id: session.organization_id, locale },
              controller.signal,
            )
          : await request<{ messages: unknown }>(
              `/localization/catalog?locale=${locale}`,
              undefined,
              undefined,
              controller.signal,
            );
        if (controller.signal.aborted) return;
        replacePublishedMessages(locale, validatePublishedMessages(result.messages, locale));
        setCatalogVersion((version) => version + 1);
      } catch {
        // Последний успешный каталог остаётся до смены организации или языка.
      } finally {
        pending = false;
      }
    }
    void refresh();
    const timer = window.setInterval(() => void refresh(), 60000);
    const visible = () => void refresh();
    document.addEventListener('visibilitychange', visible);
    return () => {
      controller.abort();
      window.clearInterval(timer);
      document.removeEventListener('visibilitychange', visible);
      clearPublishedMessages();
    };
  }, [session?.organization_id, session?.subject, locale]);

  useEffect(() => {
    document.documentElement.lang = locale;
    setDevicePreference(preference);
    try {
      localStorage.setItem('broker:language-preview', preference);
    } catch {
      /* Браузер может запретить кеш. */
    }
  }, [locale, preference]);
  return (
    <Context.Provider
      value={{
        locale,
        catalogVersion,
        preference,
        setPreference,
        translate: (key, values) => translate(locale, key, values),
      }}
    >
      {children}
    </Context.Provider>
  );
}
