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
  if (!enabled && !loading) return null;
  return (
    <div className="auto-load" ref={marker}>
      {loading && <span role="status">Загружаем записи…</span>}
      {enabled && failed && (
        <button disabled={loading} onClick={() => void onLoad()}>
          Повторить загрузку
        </button>
      )}
    </div>
  );
}
