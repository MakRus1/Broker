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
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
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
    confirmDiscard(!!ref.current?.querySelector('form[data-dirty="true"]'), onClose);
  }
  return (
    <>
      {createPortal(
        <dialog
          ref={ref}
          onSubmit={(event) => event.stopPropagation()}
          className={className}
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
  const [pending, setPending] = useState<(() => void) | null>(null);
  function confirmDiscard(dirty: boolean, discard: () => void) {
    if (dirty) setPending(() => discard);
    else discard();
  }
  const confirmation = pending && (
    <Dialog
      className="discard-dialog"
      title={translate('ui.discardTitle')}
      onClose={() => setPending(null)}
    >
      <p className="confirmation-copy">{translate('ui.discardHint')}</p>
      <div className="confirmation-actions">
        <button type="button" data-autofocus onClick={() => setPending(null)}>
          {translate('ui.stay')}
        </button>
        <button
          type="button"
          className="danger"
          onClick={() => {
            setPending(null);
            pending();
          }}
        >
          {translate('ui.discard')}
        </button>
      </div>
    </Dialog>
  );
  return { confirmDiscard, confirmation };
}
