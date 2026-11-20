import { createPortal } from 'react-dom';
import { RecordActionHost } from './recordActionHost';
import { useLocalization } from '../../../features/localization/Localization';
import { useEditorSection } from './RecordEditorSections';
import { RecordGuide } from './RecordGuide';
import type { RecordGuideKind } from './recordGuides';
import {
  Children,
  cloneElement,
  isValidElement,
  useContext,
  useId,
  useLayoutEffect,
  useRef,
  useState,
  type ReactElement,
  type ReactNode,
} from 'react';
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
  const host = useContext(RecordActionHost);
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
      {(actions || guide || host) && (
        <div className="record-layout-actions">
          {guide && <RecordGuide kind={guide} />}
          {actions && <div className="record-heading-primary-actions">{actions}</div>}
          {host && <div className="record-action-host" ref={host.setElement} />}
        </div>
      )}
    </header>
  );
}
export function RecordSection({
  title,
  collapsible = false,
  initiallyOpen = false,
  description,
  actions,
  children,
}: {
  title: string;
  collapsible?: boolean;
  initiallyOpen?: boolean;
  description?: string;
  actions?: ReactNode;
  children: ReactNode;
}) {
  const id = useId();
  const [expanded, setExpanded] = useState(initiallyOpen);
  useEditorSection(id, title);
  if (collapsible)
    return (
      <section
        id={id}
        className="record-layout-section record-layout-collapsible"
        aria-labelledby={`${id}-heading`}
      >
        <details
          open={expanded}
          onToggle={(event) => setExpanded(event.currentTarget.open)}
          onInvalidCapture={(event) => {
            event.currentTarget.open = true;
            setExpanded(true);
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
        {actions && <div className="record-section-actions">{actions}</div>}
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
  const host = useContext(RecordActionHost);
  const source = useRef<HTMLSpanElement>(null);
  const generatedFormId = useId();
  const [formId, setFormId] = useState<string>();
  const [inDialog, setInDialog] = useState(false);
  useLayoutEffect(() => {
    setInDialog(Boolean(source.current?.closest('dialog')));
    const form = source.current?.closest('form');
    if (form) {
      if (!form.id) form.id = generatedFormId;
      setFormId(form.id);
    }
  }, [generatedFormId, host?.element]);
  const controls = (nodes: ReactNode): ReactNode =>
    Children.map(nodes, (child) => {
      if (!isValidElement(child)) return child;
      const element = child as ReactElement<{ children?: ReactNode; form?: string }>;
      return cloneElement(element, {
        ...(element.type === 'button' && formId ? { form: element.props.form ?? formId } : {}),
        ...(element.props.children ? { children: controls(element.props.children) } : {}),
      });
    });
  const content = (
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
      <div className="record-layout-save-actions">
        {host?.element && !inDialog ? controls(children) : children}
      </div>
    </div>
  );
  return host?.element && !inDialog ? (
    <>
      <span ref={source} hidden />
      {createPortal(content, host.element)}
    </>
  ) : (
    <>
      <span ref={source} hidden />
      {content}
    </>
  );
}

/** Единая сетка полей редактора; широкое поле занимает всю строку. */
export function RecordFields({ children }: { children: ReactNode }) {
  return <div className="record-fields">{children}</div>;
}
