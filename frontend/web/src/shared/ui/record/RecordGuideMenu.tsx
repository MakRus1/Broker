import { RecordHelpIcon } from './RecordHelpIcon';
import { useEffect, useRef, type ReactNode } from 'react';
import { useLocalization } from '../../../features/localization/Localization';
export function RecordGuideMenu({ children }: { children: ReactNode }) {
  const { translate } = useLocalization();
  const menu = useRef<HTMLDetailsElement>(null);
  useEffect(() => {
    function dismiss(event: PointerEvent) {
      if (event.target instanceof Node && !menu.current?.contains(event.target) && menu.current)
        menu.current.open = false;
    }
    function escape(event: KeyboardEvent) {
      if (event.key === 'Escape' && menu.current?.open) {
        menu.current.open = false;
        menu.current.querySelector('summary')?.focus();
      }
    }
    document.addEventListener('pointerdown', dismiss);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', dismiss);
      document.removeEventListener('keydown', escape);
    };
  }, []);
  return (
    <details ref={menu} className="record-guide-menu">
      <summary aria-label={translate('guide.title')} title={translate('guide.title')}>
        <RecordHelpIcon />
      </summary>
      <div className="record-guide-options">{children}</div>
    </details>
  );
}
