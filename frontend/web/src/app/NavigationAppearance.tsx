import { useId, useRef, useState } from 'react';
import { useLocalization } from '../features/localization/Localization';
import type { NavigationMode } from './navigationPreference';

export function NavigationAppearance({
  mode,
  onChange,
}: {
  mode: NavigationMode;
  onChange: (mode: NavigationMode) => void;
}) {
  const { translate } = useLocalization();
  const [open, setOpen] = useState(false);
  const [position, setPosition] = useState({ left: 72, bottom: 80 });
  const id = useId();
  const panel = useRef<HTMLDivElement>(null);
  return (
    <div className="navigation-appearance">
      <button
        type="button"
        className="navigation-appearance-trigger"
        aria-label={translate('workspace.navigationAppearance')}
        aria-controls={id}
        popoverTarget={id}
        aria-expanded={open}
        onClick={(event) => {
          const rect = event.currentTarget.getBoundingClientRect();
          const sidebar = event.currentTarget.closest('aside')!.getBoundingClientRect();
          setPosition({
            left: Math.min(sidebar.right + 8, window.innerWidth - 296),
            bottom: Math.max(12, window.innerHeight - rect.bottom),
          });
        }}
      >
        <svg
          width="18"
          height="18"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.6"
          strokeLinecap="round"
          aria-hidden="true"
        >
          <path d="M4 6h16 M4 12h16 M4 18h16" />
          <circle cx="9" cy="6" r="2" fill="var(--navigation)" />
          <circle cx="15" cy="12" r="2" fill="var(--navigation)" />
          <circle cx="9" cy="18" r="2" fill="var(--navigation)" />
        </svg>
        <span className="navigation-text">{translate('workspace.navigationAppearance')}</span>
      </button>
      <div
        ref={panel}
        id={id}
        popover="auto"
        className="navigation-appearance-popover"
        style={position}
        onToggle={(event) => setOpen(event.newState === 'open')}
      >
        <span className="navigation-appearance-title">
          {translate('workspace.navigationAppearance')}
        </span>
        <div
          className="navigation-appearance-options"
          role="group"
          aria-label={translate('workspace.navigationMode')}
        >
          {(
            [
              ['auto', 'workspace.navigationAuto', 'workspace.navigationAutoHint'],
              ['compact', 'workspace.navigationCompact', 'workspace.navigationCompactHint'],
              ['expanded', 'workspace.navigationExpanded', 'workspace.navigationExpandedHint'],
            ] as const
          ).map(([value, label, hint]) => (
            <button
              key={value}
              type="button"
              aria-pressed={mode === value}
              onClick={() => {
                onChange(value);
                panel.current?.hidePopover();
              }}
            >
              <span>
                <strong>{translate(label)}</strong>
                <small>{translate(hint)}</small>
              </span>
              <span aria-hidden="true">{mode === value ? '✓' : ''}</span>
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}
