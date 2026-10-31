import type { ReactNode } from 'react';

/** Общие действия реестра находятся в шапке, отдельно от применённых фильтров. */
export function RegistryHeading({
  title,
  count,
  children,
  onRefresh,
}: {
  title: string;
  count: number;
  children?: ReactNode;
  onRefresh: () => void;
}) {
  return (
    <div className="page-heading registry-heading">
      <div>
        <h1>{title}</h1>
        <p className="hint">Загружено: {count}</p>
      </div>
      <div className="actions">
        {children}
        <button onClick={onRefresh}>Обновить</button>
      </div>
    </div>
  );
}
