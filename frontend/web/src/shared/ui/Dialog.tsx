import { parseDraft } from '../forms/useDraftRecovery';
import './dialog.css';
import { useLocalization } from '../../features/localization/Localization';
import { useEffect, useId, useRef, useState, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
export function Dialog({
  title,
  onClose,
  children,
  className,
}: {
  title: string;
  onClose: () => void;
  children: ReactNode;
  className?: string;
}) {
  const { translate } = useLocalization();
  const ref = useRef<HTMLDialogElement>(null);
  const titleId = useId();
  const startedOnBackdrop = useRef(false);
  const { confirmClose, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    const node = ref.current!;
    const previousFocus = document.activeElement;
    node.showModal();
    node.querySelector<HTMLElement>('[data-autofocus],input,textarea,select')?.focus();
    return () => {
      node.close();
      if (previousFocus instanceof HTMLElement && previousFocus.isConnected) previousFocus.focus();
    };
  }, []);
  function close() {
    if (ref.current?.querySelector('form[data-busy="true"]')) return;
    confirmClose(ref.current ? [ref.current] : [], onClose);
  }
  return (
    <>
      {createPortal(
        <dialog
          ref={ref}
          onSubmit={(event) => event.stopPropagation()}
          className={['app-dialog', className].filter(Boolean).join(' ')}
          aria-labelledby={titleId}
          onCancel={(event) => {
            event.preventDefault();
            close();
          }}
          onPointerDown={(event) => {
            const rect = event.currentTarget.getBoundingClientRect();
            startedOnBackdrop.current =
              event.target === event.currentTarget &&
              (event.clientX < rect.left ||
                event.clientX > rect.right ||
                event.clientY < rect.top ||
                event.clientY > rect.bottom);
          }}
          onClick={(event) => {
            const rect = event.currentTarget.getBoundingClientRect();
            const outside =
              event.clientX < rect.left ||
              event.clientX > rect.right ||
              event.clientY < rect.top ||
              event.clientY > rect.bottom;
            // Выделение текста и нативный select могут завершить клик на самом dialog.
            if (startedOnBackdrop.current && event.target === event.currentTarget && outside)
              close();
            startedOnBackdrop.current = false;
          }}
        >
          <div className="dialog-heading">
            <h2 id={titleId}>{title}</h2>
            <button type="button" aria-label={translate('ui.close')} onClick={close}>
              ×
            </button>
          </div>
          {children}
        </dialog>,
        document.body,
      )}
      {confirmation}
    </>
  );
}

export function useDiscardConfirmation() {
  const { translate } = useLocalization();
  const [pending, setPending] = useState<{ action: () => void; keepDraft: boolean } | null>(null);
  function confirmDiscard(dirty: boolean, discard: () => void, keepDraft = false) {
    if (dirty) setPending({ action: discard, keepDraft });
    else discard();
  }
  const confirmation = pending && (
    <Dialog
      className="discard-dialog"
      title={translate(pending.keepDraft ? 'ui.closeDraftTitle' : 'ui.discardTitle')}
      onClose={() => setPending(null)}
    >
      <p className="confirmation-copy">
        {translate(pending.keepDraft ? 'ui.closeDraftHint' : 'ui.discardHint')}
      </p>
      <DialogActions>
        <button type="button" data-autofocus onClick={() => setPending(null)}>
          {translate(pending.keepDraft ? 'ui.continueEditing' : 'ui.stay')}
        </button>
        <button
          type="button"
          className={pending.keepDraft ? undefined : 'danger'}
          onClick={() => {
            setPending(null);
            pending.action();
          }}
        >
          {translate(pending.keepDraft ? 'ui.close' : 'ui.discard')}
        </button>
      </DialogActions>
    </Dialog>
  );
  function confirmClose(nodes: HTMLElement[], close: () => void) {
    const forms = nodes.flatMap((node) => [
      ...node.querySelectorAll<HTMLElement>('form[data-dirty="true"]'),
    ]);
    const keepDraft =
      forms.length > 0 &&
      forms.every((form) => {
        try {
          return (
            !!form.dataset.recoveryKey &&
            !!parseDraft(localStorage.getItem(form.dataset.recoveryKey))
          );
        } catch {
          return false;
        }
      });
    confirmDiscard(
      forms.length > 0,
      () => {
        // Если хранилище недоступно, явно предупреждаем о потере изменений.
        if (forms.length && !keepDraft) {
          for (const node of nodes)
            node
              .querySelectorAll<HTMLElement>('[data-recovery-key], [data-draft-key]')
              .forEach((form) => {
                try {
                  if (form.dataset.recoveryKey) localStorage.removeItem(form.dataset.recoveryKey);
                  if (form.dataset.draftKey) sessionStorage.removeItem(form.dataset.draftKey);
                } catch {
                  /* Закрытие доступно и без хранилища. */
                }
              });
        }
        close();
      },
      keepDraft,
    );
  }
  return { confirmDiscard, confirmClose, confirmation };
}

export function DialogActions({ children, error }: { children: ReactNode; error?: string }) {
  return (
    <div className="dialog-actions">
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      <div className="dialog-action-buttons">{children}</div>
    </div>
  );
}
