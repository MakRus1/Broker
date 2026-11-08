import { useLocalization } from '../../../features/localization/Localization';
import { useEditorSection } from './RecordEditorSections';
import { RecordGuide } from './RecordGuide';
import type { RecordGuideKind } from './recordGuides';
import { useId, type ReactNode } from 'react';
import './recordLayout.css';

export function RecordHeading({
  kind,
  title,
  status,
  metadata,
  mode,
  actions,
  guide,
}: {
  kind: string;
  title: string;
  status?: ReactNode;
  metadata?: ReactNode;
  mode?: string;
  actions?: ReactNode;
  guide?: RecordGuideKind;
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
      {(actions || guide) && (
        <div className="record-layout-actions">
          {guide && <RecordGuide kind={guide} />}
          {actions}
        </div>
      )}
    </header>
  );
}
export function RecordSection({
  title,
  collapsible = false,
  description,
  children,
}: {
  title: string;
  collapsible?: boolean;
  description?: string;
  children: ReactNode;
}) {
  const id = useId();
  useEditorSection(id, title);
  if (collapsible)
    return (
      <section
        id={id}
        className="record-layout-section record-layout-collapsible"
        aria-labelledby={`${id}-heading`}
      >
        <details
          open
          onInvalidCapture={(event) => {
            event.currentTarget.open = true;
          }}
        >
          <summary>
            <h2 id={`${id}-heading`} tabIndex={-1}>
              {title}
            </h2>
          </summary>
          <div className="record-layout-section-body">{children}</div>
        </details>
      </section>
    );
  return (
    <section id={id} className="record-layout-section" aria-labelledby={`${id}-heading`}>
      <div className="record-layout-section-heading">
        <h2 id={`${id}-heading`} tabIndex={-1}>
          {title}
        </h2>
        {description && <p>{description}</p>}
      </div>
      <div className="record-layout-section-body">{children}</div>
    </section>
  );
}
export function RecordActions({
  status,
  error,
  issues,
  onIssueClick,
  children,
}: {
  status: ReactNode;
  error?: string;
  issues?: {
    field: HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement;
    label: string;
    message: string;
  }[];
  onIssueClick?: (field: HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement) => void;
  children: ReactNode;
}) {
  const { translate } = useLocalization();
  return (
    <div className="record-layout-save">
      {error && (
        <p className="record-layout-save-error error" role="alert">
          {error}
        </p>
      )}
      {issues && issues.length > 0 && (
        <div className="record-validation" aria-live="polite">
          <strong>{translate('validation.summaryTitle')}</strong>
          <p>{translate('validation.summaryHint')}</p>
          <ul className="record-validation-issues" role="list">
            {issues.map(({ field, label, message }, index) => (
              <li key={index}>
                <button type="button" onClick={() => onIssueClick?.(field)}>
                  {label || message}
                </button>
                {label && <span>{message}</span>}
              </li>
            ))}
          </ul>
        </div>
      )}
      <span className="record-layout-save-status" role="status">
        {status}
      </span>
      <div className="record-layout-save-actions">{children}</div>
    </div>
  );
}
