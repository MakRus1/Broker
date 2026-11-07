import { RecordHelpIcon } from './RecordHelpIcon';
import { useEffect, useRef, useState } from 'react';
import {
  usePreferences,
  usePreferencesSession,
} from '../../../features/preferences/UserPreferences';
import { claimGuideNotice } from './guideNotice';
import { useLocalization } from '../../../features/localization/Localization';
export function RecordGuideNotice({
  context,
  active,
  editing,
}: {
  context: string;
  active: boolean;
  editing: boolean;
}) {
  const [session] = usePreferencesSession();
  const { translate } = useLocalization();
  const [visible, setVisible] = useState(false);
  const preferences = usePreferences();
  const shown = useRef('');
  const owner = session ? `${session.actor}:${session.subject}:${session.organization_id}` : '';
  useEffect(() => {
    setVisible(false);
    if (!session || !preferences || !active || !context) {
      shown.current = '';
      return;
    }
    const presentation = `${owner}:${context}`;
    if (
      shown.current !== presentation &&
      !claimGuideNotice(preferences, session.subject, editing)
    ) {
      shown.current = '';
      return;
    }
    shown.current = presentation;
    setVisible(true);
    const timer = window.setTimeout(() => {
      shown.current = '';
      setVisible(false);
    }, 10000);
    return () => window.clearTimeout(timer);
  }, [context, active, owner, editing, preferences]);
  if (!visible || !active) return null;
  return (
    <span className="record-guide-notice" role="status">
      {translate(editing ? 'guide.fillingNotice' : 'guide.editorNotice')}{' '}
      <button
        className="record-guide-notice-help"
        type="button"
        aria-label={translate('guide.title')}
        onClick={(event) => {
          const menu = event.currentTarget
            .closest('.record-guide-control')
            ?.querySelector('details');
          if (menu) menu.open = true;
          shown.current = '';
          setVisible(false);
        }}
      >
        <RecordHelpIcon />
      </button>
      <button
        type="button"
        className="record-guide-notice-close"
        aria-label={translate('ui.close')}
        onClick={() => {
          setVisible(false);
          shown.current = '';
        }}
      >
        <svg
          width="16"
          height="16"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          aria-hidden="true"
        >
          <path d="m6 6 12 12M18 6 6 18" />
        </svg>
      </button>
    </span>
  );
}
