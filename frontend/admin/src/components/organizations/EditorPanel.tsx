import { useEffect, useRef, useState, type ReactNode } from 'react';

export function EditorPanel({
  title,
  dirty,
  busy,
  close,
  children,
}: {
  title: string;
  dirty: boolean;
  busy: boolean;
  close: () => void;
  children: ReactNode;
}) {
  const dialog = useRef<HTMLDialogElement>(null);
  const [discard, setDiscard] = useState(false);
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null;
    const element = dialog.current;
    element?.showModal();
    return () => {
      element?.close();
      previous?.focus();
    };
  }, []);
  function requestClose() {
    if (busy) return;
    if (dirty) setDiscard(true);
    else close();
  }
  return (
    <dialog
      ref={dialog}
      className="organization-editor"
      aria-labelledby="organization-editor-title"
      onCancel={(event) => {
        event.preventDefault();
        requestClose();
      }}
      onClick={(event) => {
        if (event.target !== event.currentTarget) return;
        const bounds = event.currentTarget.getBoundingClientRect();
        if (
          event.clientX < bounds.left ||
          event.clientX > bounds.right ||
          event.clientY < bounds.top ||
          event.clientY > bounds.bottom
        )
          requestClose();
      }}
    >
      <div className="organization-editor-heading">
        <h2 id="organization-editor-title">{title}</h2>
        <button type="button" aria-label="Закрыть форму" disabled={busy} onClick={requestClose}>
          ✕
        </button>
      </div>
      {discard && (
        <div className="organization-discard" role="alert">
          <p>Есть несохранённые изменения.</p>
          <div className="actions">
            <button type="button" onClick={() => setDiscard(false)}>
              Продолжить редактирование
            </button>
            <button type="button" className="danger" onClick={close}>
              Не сохранять
            </button>
          </div>
        </div>
      )}
      {children}
    </dialog>
  );
}
