import { useMemo, useState, type ComponentPropsWithRef } from 'react';
import { RecordActionHost } from './recordActionHost';

/** Общая поверхность карточки в режимах создания, просмотра и редактирования. */
export function RecordPage({ className = '', ...props }: ComponentPropsWithRef<'section'>) {
  const [element, setElement] = useState<HTMLDivElement | null>(null);
  const host = useMemo(() => ({ element, setElement }), [element]);
  return (
    <RecordActionHost.Provider value={host}>
      <section {...props} className={`surface record-workbench ${className}`} />
    </RecordActionHost.Provider>
  );
}

/** Рабочая область записи и её реквизиты используют один порядок на всех экранах. */
export function RecordOverview({ children, ...props }: ComponentPropsWithRef<'div'>) {
  return (
    <div {...props} className="record-overview">
      {children}
    </div>
  );
}

export function RecordContext({ children, ...props }: ComponentPropsWithRef<'section'>) {
  return (
    <section {...props} className="record-context">
      {children}
    </section>
  );
}
