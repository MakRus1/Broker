import { useEffect, useRef, useState } from 'react';
import { useLocalization } from './Localization';
export function LanguageControl() {
  const { translate, preference, setPreference } = useLocalization();
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
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
        aria-label={translate('language.title')}
        title={translate('language.title')}
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
          <circle cx="12" cy="12" r="9" />
          <ellipse cx="12" cy="12" rx="4" ry="9" />
          <path d="M3 12h18" />
        </svg>
      </button>
      {open && (
        <div className="theme-menu" role="group" aria-label={translate('language.title')}>
          <span className="theme-menu-title">{translate('language.title')}</span>
          {[
            { value: 'auto', label: translate('language.auto') },
            { value: 'ru', label: 'Русский' },
            { value: 'en', label: 'English' },
          ].map((item) => (
            <button
              type="button"
              key={item.value}
              aria-pressed={preference === item.value}
              onClick={() => setPreference(item.value)}
            >
              {item.label}
              <span aria-hidden="true">{preference === item.value ? '✓' : ''}</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
