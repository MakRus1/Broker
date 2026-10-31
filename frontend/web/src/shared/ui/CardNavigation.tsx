import { useState, type ReactNode } from 'react';
export function CardNavigation({
  children,
  onRefresh,
  disabled = false,
}: {
  children: ReactNode;
  onRefresh?: () => Promise<unknown>;
  disabled?: boolean;
}) {
  const [refreshing, setRefreshing] = useState(false);
  return (
    <div className="card-navigation">
      <div className="tabs">{children}</div>
      {onRefresh && (
        <button
          type="button"
          className="card-refresh"
          disabled={disabled || refreshing}
          onClick={async () => {
            setRefreshing(true);
            try {
              await onRefresh();
            } finally {
              setRefreshing(false);
            }
          }}
        >
          <span aria-hidden="true">↻</span> {refreshing ? 'Обновляем…' : 'Обновить'}
        </button>
      )}
    </div>
  );
}
