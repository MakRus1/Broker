import { createContext, useContext, useEffect, useState, type ReactNode } from 'react';
import {
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
  translate: (key: MessageKey, values?: Record<string, string | number>) =>
    translate('ru', key, values),
  preference: 'auto',
  setPreference: (_value: string) => {},
});
export const useLocalization = () => useContext(Context);
export function Localization({ children }: { children: ReactNode }) {
  const [session] = usePreferencesSession();
  const [preference, setPreference] = useRegistryPreference(
    preferenceKey(session?.subject ?? '', 'appearance', 'language'),
    initialLanguagePreference(),
    restoreLanguage,
  );
  const [languages, setLanguages] = useState(() => navigator.languages);
  useEffect(() => {
    const changed = () => setLanguages(navigator.languages);
    window.addEventListener('languagechange', changed);
    return () => window.removeEventListener('languagechange', changed);
  }, []);
  const locale = resolveLocale(preference, languages);
  useEffect(() => {
    document.documentElement.lang = locale;
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
        preference,
        setPreference: (value) => setPreference(restoreLanguage(value)),
        translate: (key, values) => translate(locale, key, values),
      }}
    >
      {children}
    </Context.Provider>
  );
}
