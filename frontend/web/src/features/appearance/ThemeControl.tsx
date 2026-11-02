import { useLocalization } from '../localization/Localization';
import '../../../../shared/theme/controls.css';
import { useEffect, useRef, useState } from 'react';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { usePreferencesSession } from '../preferences/UserPreferences';
import { restoreTheme, resolveTheme, type ThemePreference } from '../../../../shared/theme/theme';

export function ThemeControl() {
  const { translate } = useLocalization();
  const [session] = usePreferencesSession();
  const [theme, setTheme] = useRegistryPreference(
    preferenceKey(session?.subject ?? '', 'appearance', 'theme'),
    'system' as ThemePreference,
    restoreTheme,
  );
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
  useEffect(() => {
    const system = window.matchMedia('(prefers-color-scheme: dark)');
    const apply = () => {
      document.documentElement.dataset.theme = resolveTheme(theme, system.matches);
      try {
        localStorage.setItem('broker:theme-preview', theme);
      } catch {
        /* Не мешает переключению. */
      }
    };
    apply();
    system.addEventListener('change', apply);
    return () => system.removeEventListener('change', apply);
  }, [theme]);
  useEffect(() => {
    const outside = (event: PointerEvent) => {
      if (!ref.current?.contains(event.target as Node)) setOpen(false);
    };
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
  return (
    <div
      className="theme-control"
      ref={ref}
      onKeyDown={(event) => {
        if (event.key === 'Escape') {
          setOpen(false);
          trigger.current?.focus();
        }
      }}
    >
      <button
        type="button"
        className="theme-trigger"
        ref={trigger}
        aria-label={translate('theme.title')}
        title={translate('theme.title')}
        aria-expanded={open}
        onClick={() => setOpen(!open)}
      >
        <svg
          width="18"
          height="18"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.7"
          aria-hidden="true"
        >
          <circle cx="12" cy="12" r="8" />
          <path d="M12 4a8 8 0 0 1 0 16Z" fill="currentColor" stroke="none" />
        </svg>
      </button>
      {open && (
        <div className="theme-menu" role="group" aria-label={translate('theme.title')}>
          <span className="theme-menu-title">{translate('theme.title')}</span>
          {(
            [
              { value: 'light', label: translate('theme.light') },
              { value: 'dark', label: translate('theme.dark') },
              { value: 'system', label: translate('theme.system') },
            ] as const
          ).map((item) => (
            <button
              key={item.value}
              type="button"
              aria-pressed={theme === item.value}
              onClick={() => setTheme(item.value)}
            >
              {item.label}
              <span aria-hidden="true">{theme === item.value ? '✓' : ''}</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
