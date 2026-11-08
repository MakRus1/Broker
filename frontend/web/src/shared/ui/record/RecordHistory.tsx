import type { ReactNode } from 'react';
import { useLocalization } from '../../../features/localization/Localization';
import './recordHistory.css';

export function RecordHistoryEntry({
  title,
  actor,
  actorTitle,
  occurredAt,
  revision,
  initiallyOpen,
  children,
}: {
  title: string;
  actor: ReactNode;
  actorTitle?: string;
  occurredAt: string;
  revision: string;
  initiallyOpen: boolean;
  children: ReactNode;
}) {
  const { locale } = useLocalization();
  return (
    <details className="record-history-event" open={initiallyOpen || undefined}>
      <summary>
        <span className="record-history-title">{title}</span>
        <span className="record-history-actor" title={actorTitle}>
          {actor}
        </span>
        <time dateTime={occurredAt}>{new Date(occurredAt).toLocaleString(locale)}</time>
        <span className="hint">{revision}</span>
      </summary>
      {children}
    </details>
  );
}

export type HistoryDiffRow = { key: string; label: string; before: ReactNode; after: ReactNode };

export function RecordHistoryDiff({ rows }: { rows: HistoryDiffRow[] }) {
  const { translate } = useLocalization();
  if (!rows.length) return <p className="hint">{translate('counterparty.noFieldValuesChanged')}</p>;
  return (
    <div className="table-scroll">
      <table className="record-history-diff">
        <thead>
          <tr>
            <th>{translate('counterparty.field')}</th>
            <th>{translate('counterparty.before')}</th>
            <th>{translate('counterparty.after')}</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((row) => (
            <tr key={row.key}>
              <th scope="row">{row.label}</th>
              <td>{row.before}</td>
              <td>{row.after}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
