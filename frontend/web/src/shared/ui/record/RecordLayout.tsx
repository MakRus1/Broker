import { useId, type ReactNode } from 'react';
import './recordLayout.css';

export function RecordHeading({
  kind,
  title,
  status,
  metadata,
  mode,
  actions,
}: {
  kind: string;
  title: string;
  status?: ReactNode;
  metadata?: ReactNode;
  mode?: string;
  actions?: ReactNode;
}) {
  return (
    <header className="record-layout-heading">
      <div className="record-layout-identity">
        <div className="record-layout-kind">
          <span>{kind}</span>
          {status && <span className="record-layout-status">{status}</span>}
        </div>
        <h1>{title}</h1>
        {metadata && <p className="record-layout-metadata">{metadata}</p>}
        {mode && <p className="record-layout-mode">{mode}</p>}
      </div>
      {actions && <div className="record-layout-actions">{actions}</div>}
    </header>
  );
}
export function RecordSection({
  title,
  description,
  children,
}: {
  title: string;
  description?: string;
  children: ReactNode;
}) {
  const id = useId();
  return (
    <section className="record-layout-section" aria-labelledby={id}>
      <div className="record-layout-section-heading">
        <h2 id={id}>{title}</h2>
        {description && <p>{description}</p>}
      </div>
      {children}
    </section>
  );
}
export function RecordActions({
  status,
  error,
  children,
}: {
  status: ReactNode;
  error?: string;
  children: ReactNode;
}) {
  return (
    <div className="record-layout-save">
      {error && (
        <p className="record-layout-save-error error" role="alert">
          {error}
        </p>
      )}
      <span className="record-layout-save-status" role="status">
        {status}
      </span>
      <div className="record-layout-save-actions">{children}</div>
    </div>
  );
}
