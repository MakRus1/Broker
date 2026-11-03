import { useLocalization } from '../../../features/localization/Localization';
import type { DraftEnvelope } from '../../forms/useDraftRecovery';
export function DraftRecovery({
  recovery,
}: {
  recovery: {
    candidate: DraftEnvelope | null;
    stale: boolean;
    failed: boolean;
    saved: boolean;
    restore: () => void;
    discard: () => void;
  };
}) {
  const { translate, locale } = useLocalization();
  if (recovery.candidate)
    return (
      <aside className="draft-recovery" aria-label={translate('draft.found')}>
        <div>
          <strong>{translate(recovery.stale ? 'draft.stale' : 'draft.found')}</strong>
          <p>
            {translate(recovery.stale ? 'draft.staleHint' : 'draft.hint')}{' '}
            <time dateTime={new Date(recovery.candidate.savedAt).toISOString()}>
              {new Intl.DateTimeFormat(locale, { dateStyle: 'short', timeStyle: 'short' }).format(
                recovery.candidate.savedAt,
              )}
            </time>
          </p>
        </div>
        <div className="draft-recovery-actions">
          {!recovery.stale && (
            <button type="button" onClick={recovery.restore}>
              {translate('draft.restore')}
            </button>
          )}
          <button type="button" onClick={recovery.discard}>
            {translate('draft.discard')}
          </button>
        </div>
      </aside>
    );
  return (
    <>
      {recovery.failed && (
        <p className="hint" role="status">
          {translate('draft.storageFailed')}
        </p>
      )}
      {recovery.saved && (
        <p className="record-save-confirmation" role="status">
          {translate('draft.saved')}
        </p>
      )}
    </>
  );
}
