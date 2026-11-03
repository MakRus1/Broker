import { RecordHelpIcon } from './RecordHelpIcon';
import type { ReactNode } from 'react';
import { useLocalization } from '../../../features/localization/Localization';
export function RecordGuideMenu({ children }: { children: ReactNode }) {
  const { translate } = useLocalization();
  return (
    <details className="record-guide-menu">
      <summary aria-label={translate('guide.title')} title={translate('guide.title')}>
        <RecordHelpIcon />
      </summary>
      <div className="record-guide-options">{children}</div>
    </details>
  );
}
