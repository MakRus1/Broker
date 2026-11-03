import { RecordHelpIcon } from './RecordHelpIcon';
import { useEffect, useState } from 'react';
import { usePreferencesSession } from '../../../features/preferences/UserPreferences';
import {
  preferenceKey,
  useRegistryPreference,
} from '../../../features/registry/registryPreferences';
import { useLocalization } from '../../../features/localization/Localization';
export function RecordGuideNotice({
  kind,
  context,
  active,
  editing,
}: {
  kind: string;
  context: string;
  active: boolean;
  editing: boolean;
}) {
  const [session] = usePreferencesSession();
  const { translate } = useLocalization();
  const [visible, setVisible] = useState(false);
  const [seen, setSeen] = useRegistryPreference<string[]>(
    preferenceKey(session?.subject ?? '', `guide-${kind}`, 'seen-contexts-v3'),
    [],
    (raw) =>
      Array.isArray(raw) ? raw.filter((value): value is string => typeof value === 'string') : [],
  );
  useEffect(() => {
    setVisible(false);
    if (!session || !active || !context || seen.includes(context)) return;
    setVisible(true);
    const timer = window.setTimeout(() => {
      setVisible(false);
      setSeen((old) => [...new Set([...old, context])]);
    }, 10000);
    return () => window.clearTimeout(timer);
  }, [context, active, session?.subject]);
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
          setSeen((old) => [...new Set([...old, context])]);
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
