import { useRegistryPreference } from '../../features/registry/registryPreferences';
import { useEffect, useRef, useState } from 'react';
import { useLocalization } from '../../features/localization/Localization';

/** An inline disclosure works with mouse, keyboard and touch without a hover-only tooltip. */
export function RegistryGuide() {
  const { translate } = useLocalization();
  const details = useRef<HTMLDetailsElement>(null);
  const [open, setOpen] = useState(false);
  useEffect(() => {
    if (!open) return;
    const outside = (event: PointerEvent) => {
      if (details.current && !details.current.contains(event.target as Node))
        details.current.open = false;
    };
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, [open]);
  return (
    <details
      ref={details}
      className="registry-guide"
      onToggle={(event) => setOpen(event.currentTarget.open)}
      onKeyDown={(event) => {
        if (event.key === 'Escape') {
          event.currentTarget.open = false;
          event.currentTarget.querySelector('summary')?.focus();
        }
      }}
    >
      <summary>
        <svg
          aria-hidden="true"
          width="17"
          height="17"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.6"
        >
          <circle cx="12" cy="12" r="9" />
          <path d="M9.5 9a2.5 2.5 0 0 1 5 0c0 2-2.5 2-2.5 4M12 16v1" />
        </svg>
        {translate('registry.help')}
      </summary>
      <div className="registry-guide-content">
        <strong>{translate('registry.guideTitle')}</strong>
        <p>{translate('registry.guideFilters')}</p>
        <p>{translate('registry.guideColumns')}</p>
        <p>{translate('registry.guideSaved')}</p>
        <p>{translate('registry.openRow')}</p>
      </div>
    </details>
  );
}
export function RegistryEmptyState({
  kind,
  title,
  description,
  action,
}: {
  kind: 'empty' | 'filtered' | 'failure';
  title: string;
  description: string;
  action?: { label: string; onClick: () => void };
}) {
  return (
    <div className="registry-empty-state" role={kind === 'failure' ? 'alert' : 'status'}>
      <svg
        aria-hidden="true"
        width="28"
        height="28"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.3"
      >
        {kind === 'filtered' ? (
          <>
            <circle cx="10" cy="10" r="6" />
            <path d="m15 15 5 5M7 10h6" />
          </>
        ) : kind === 'failure' ? (
          <>
            <circle cx="12" cy="12" r="9" />
            <path d="M12 7v6M12 16v1" />
          </>
        ) : (
          <>
            <rect x="3" y="4" width="18" height="16" rx="2" />
            <path d="M3 9h18M8 9v11" />
          </>
        )}
      </svg>
      <h2>{title}</h2>
      <p>{description}</p>
      {action && (
        <button type="button" onClick={action.onClick}>
          {action.label}
        </button>
      )}
    </div>
  );
}

export function RegistryTip({ preference }: { preference: string }) {
  const { translate } = useLocalization();
  const [dismissed, setDismissed] = useRegistryPreference(preference, false, (raw) => raw === true);
  if (dismissed) return null;
  return (
    <aside className="registry-tip">
      <svg
        aria-hidden="true"
        width="16"
        height="16"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.6"
      >
        <circle cx="12" cy="12" r="9" />
        <path d="M12 11v6M12 7v1" />
      </svg>
      <span>{translate('registry.quickTip')}</span>
      <button
        type="button"
        aria-label={translate('registry.dismissGuide')}
        onClick={() => setDismissed(true)}
      >
        <svg
          aria-hidden="true"
          width="14"
          height="14"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
        >
          <path d="m6 6 12 12M18 6 6 18" />
        </svg>
      </button>
    </aside>
  );
}
export function RegistryResultState({
  loading,
  failed,
  failureMessage,
  filtered,
  title,
  onRetry,
  onReset,
  create,
}: {
  loading: boolean;
  failed: boolean;
  failureMessage?: string;
  filtered: boolean;
  title: string;
  onRetry: () => void;
  onReset: () => void;
  create?: { label: string; onClick: () => void };
}) {
  const { translate } = useLocalization();
  if (loading)
    return (
      <p className="registry-loading" role="status">
        {translate('common.loading')}
      </p>
    );
  return (
    <RegistryEmptyState
      kind={failed ? 'failure' : filtered ? 'filtered' : 'empty'}
      title={
        failed
          ? translate('registry.loadFailed')
          : filtered
            ? translate('registry.noMatches')
            : title
      }
      description={
        failed && failureMessage
          ? failureMessage
          : translate(
              failed
                ? 'registry.loadFailedHint'
                : filtered
                  ? 'registry.noMatchesHint'
                  : create
                    ? 'registry.emptyCreateHint'
                    : 'registry.emptyReadOnlyHint',
            )
      }
      action={
        failed
          ? { label: translate('filter.retry'), onClick: onRetry }
          : filtered
            ? { label: translate('filter.reset'), onClick: onReset }
            : create
      }
    />
  );
}
