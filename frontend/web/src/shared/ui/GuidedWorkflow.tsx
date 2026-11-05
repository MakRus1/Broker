import { useEffect, useId, useLayoutEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { useRegistryPreference } from '../../features/registry/registryPreferences';
import { useLocalization } from '../../features/localization/Localization';
import type { MessageKey } from '../../../../shared/i18n/catalog';
import './guidedWorkflow.css';

export type WorkflowStep = {
  id: string;
  title: MessageKey;
  titleText?: string;
  description: MessageKey;
};
type Progress = { status: 'ready' | 'active' | 'skipped' | 'completed'; step: string };
export function restoreWorkflow(raw: unknown, steps: readonly WorkflowStep[]): Progress {
  const value = raw as Partial<Progress> | null;
  return {
    status:
      value && ['active', 'skipped', 'completed'].includes(value.status ?? '')
        ? (value.status as Progress['status'])
        : 'ready',
    step:
      value?.status !== 'ready' && steps.some((step) => step.id === value?.step)
        ? value!.step!
        : (steps[0]?.id ?? ''),
  };
}

/** A non-modal coachmark: navigation is guarded by the owning form. */
export function GuidedWorkflow({
  preference,
  steps,
  onOpen,
  getTarget,
  active = true,
  title = 'workflow.title',
  textButton = false,
}: {
  preference: string;
  steps: readonly WorkflowStep[];
  onOpen: (id: string, proceed: () => void) => void;
  getTarget: (id: string) => HTMLElement | null;
  active?: boolean;
  title?: MessageKey;
  textButton?: boolean;
}) {
  const { translate } = useLocalization();
  const [progress, setProgress] = useRegistryPreference<Progress>(
    preference,
    { status: 'ready', step: steps[0]?.id ?? '' },
    (raw) => restoreWorkflow(raw, steps),
  );
  const [open, setOpen] = useState(false);
  const [moving, setMoving] = useState(false);
  useEffect(() => {
    if (!moving) return;
    const timeout = window.setTimeout(() => setMoving(false), 220);
    return () => window.clearTimeout(timeout);
  }, [moving, progress.step]);
  const opener = useRef<HTMLButtonElement>(null);
  const card = useRef<HTMLDivElement>(null);
  const targetFor = useRef(getTarget);
  targetFor.current = getTarget;
  const titleId = useId();
  const descriptionId = useId();
  const [position, setPosition] = useState({ left: 16, top: 16 });
  const index = Math.max(
    0,
    progress.status === 'ready' ? 0 : steps.findIndex((step) => step.id === progress.step),
  );
  const step = steps[index];
  const visible = open && active && !!step;
  function close() {
    setOpen(false);
    const menu = opener.current?.closest('details');
    const focusTarget =
      menu && !menu.open ? menu.querySelector<HTMLElement>('summary') : opener.current;
    focusTarget?.focus({ preventScroll: true });
  }
  useEffect(() => {
    if (!visible) return;
    const escape = (event: KeyboardEvent) => {
      if (
        !event.defaultPrevented &&
        event.key === 'Escape' &&
        !document.querySelector('dialog[open]')
      ) {
        event.preventDefault();
        close();
      }
    };
    document.addEventListener('keydown', escape);
    return () => document.removeEventListener('keydown', escape);
  }, [visible]);
  useLayoutEffect(() => {
    if (!visible) return;
    const target = targetFor.current(step.id);
    target?.classList.add('workflow-target');
    target?.scrollIntoView({
      block: 'nearest',
      inline: 'nearest',
      behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches
        ? 'instant'
        : 'smooth',
    });
    const update = () => {
      const rect = target?.getBoundingClientRect() ?? opener.current?.getBoundingClientRect();
      if (!rect || !card.current) return;
      const width = card.current.offsetWidth;
      const height = card.current.offsetHeight;
      const left = Math.max(12, Math.min(rect.left, window.innerWidth - width - 12));
      const below = rect.bottom + 10;
      const preferred = below + height <= window.innerHeight - 12 ? below : rect.top - height - 10;
      const top = Math.max(12, Math.min(preferred, window.innerHeight - height - 12));
      setPosition((old) => (old.left === left && old.top === top ? old : { left, top }));
    };
    update();
    const observer = new ResizeObserver(update);
    if (target) observer.observe(target);
    if (card.current) observer.observe(card.current);
    window.addEventListener('resize', update);
    window.addEventListener('scroll', update, true);
    return () => {
      target?.classList.remove('workflow-target');
      observer.disconnect();
      window.removeEventListener('resize', update);
      window.removeEventListener('scroll', update, true);
    };
  }, [visible, step?.id]);
  if (!steps.length) return null;
  function show(next: number) {
    onOpen(steps[next].id, () => {
      setMoving(open && steps[next].id !== step.id);
      setProgress({ status: 'active', step: steps[next].id });
      setOpen(true);
    });
  }
  return (
    <>
      <button
        ref={opener}
        type="button"
        className={textButton ? 'workflow-help workflow-help-text' : 'workflow-help'}
        aria-label={translate(title)}
        title={translate(title)}
        aria-haspopup="dialog"
        aria-expanded={visible}
        onClick={() => {
          const menu = opener.current?.closest('details');
          if (menu) menu.open = false;
          if (visible) close();
          else show(progress.status === 'completed' ? 0 : index);
        }}
      >
        {textButton ? (
          translate(title)
        ) : (
          <svg
            width="18"
            height="18"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="1.7"
            aria-hidden="true"
          >
            <circle cx="12" cy="12" r="9" />
            <path d="M9.5 9a2.5 2.5 0 0 1 5 0c0 2-2.5 2-2.5 4M12 16v1" />
          </svg>
        )}
      </button>
      {visible &&
        createPortal(
          <div
            ref={card}
            className="workflow-coachmark"
            data-moving={moving}
            role="dialog"
            aria-modal="false"
            aria-labelledby={titleId}
            aria-describedby={descriptionId}
            style={{ transform: `translate3d(${position.left}px, ${position.top}px, 0)` }}
          >
            <div className="workflow-coachmark-heading">
              <span>
                {translate('workflow.progress', { current: index + 1, total: steps.length })}
              </span>
              <button
                type="button"
                className="workflow-close"
                aria-label={translate('ui.close')}
                onClick={close}
              >
                <svg
                  width="16"
                  height="16"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="2"
                  strokeLinecap="round"
                  aria-hidden="true"
                >
                  <path d="m6 6 12 12M18 6 6 18" />
                </svg>
              </button>
            </div>
            <div
              key={step.id}
              className="workflow-step-content"
              aria-live="polite"
              aria-atomic="true"
            >
              <h3 id={titleId}>{step.titleText ?? translate(step.title)}</h3>
              <p id={descriptionId}>{translate(step.description)}</p>
            </div>
            <div className="workflow-coachmark-actions">
              {index > 0 && (
                <button type="button" onClick={() => show(index - 1)}>
                  {translate('workflow.previous')}
                </button>
              )}
              <button
                type="button"
                className="primary"
                onClick={() => {
                  if (index + 1 < steps.length) show(index + 1);
                  else {
                    setProgress({ ...progress, status: 'completed' });
                    close();
                  }
                }}
              >
                {translate(index + 1 < steps.length ? 'workflow.next' : 'workflow.finish')}
              </button>
            </div>
          </div>,
          document.body,
        )}
    </>
  );
}
