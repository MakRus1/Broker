import { useDelayedBusy } from './useDelayedBusy';
import { useLocalization } from '../../features/localization/Localization';
import { useEffect, useRef } from 'react';

export function AutoLoadMore({
  enabled,
  loading,
  failed = false,
  onLoad,
}: {
  enabled: boolean;
  loading: boolean;
  failed?: boolean;
  onLoad: () => Promise<void>;
}) {
  const { translate } = useLocalization();
  const showLoading = useDelayedBusy(loading);
  const marker = useRef<HTMLDivElement>(null);
  const pending = useRef(false);
  useEffect(() => {
    if (!enabled || loading || failed || !marker.current) return;
    const observer = new IntersectionObserver(
      (entries) => {
        if (!entries.some((entry) => entry.isIntersecting) || pending.current) return;
        pending.current = true;
        observer.disconnect();
        void onLoad().finally(() => {
          pending.current = false;
        });
      },
      { rootMargin: '240px' },
    );
    observer.observe(marker.current);
    return () => observer.disconnect();
  }, [enabled, loading, failed, onLoad]);
  return (
    <div className="auto-load" ref={marker}>
      {showLoading && <span role="status">{translate('ui.loadingRecords')}</span>}
      {enabled && failed && (
        <button disabled={loading} onClick={() => void onLoad()}>
          {translate('ui.retryLoad')}
        </button>
      )}
    </div>
  );
}
