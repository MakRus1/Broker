import { useEffect, useRef } from 'react';
import type { Workspace } from '../hooks/useWorkspace';

function Confirmation({ workspace }: { workspace: Workspace }) {
  const dialog = useRef<HTMLDialogElement>(null);
  const { keepEditing, confirmDiscard } = workspace;
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null;
    const element = dialog.current;
    element?.showModal();
    return () => {
      element?.close();
      previous?.focus();
    };
  }, []);
  return (
    <dialog
      ref={dialog}
      className="discard-confirmation"
      aria-labelledby="discard-title"
      aria-describedby="discard-description"
      onCancel={(event) => {
        event.preventDefault();
        event.stopPropagation();
        keepEditing();
      }}
    >
      <h2 id="discard-title">Не сохранять изменения?</h2>
      <p id="discard-description">Несохранённые изменения в редакторе будут потеряны.</p>
      <div className="actions">
        <button type="button" className="primary" onClick={keepEditing}>
          Продолжить редактирование
        </button>
        <button type="button" className="danger" onClick={confirmDiscard}>
          Не сохранять
        </button>
      </div>
    </dialog>
  );
}

export function DiscardConfirmation({ workspace }: { workspace: Workspace }) {
  return workspace.discardPending ? <Confirmation workspace={workspace} /> : null;
}
