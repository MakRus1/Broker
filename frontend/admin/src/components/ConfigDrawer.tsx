import { useEffect, useRef, useState } from 'react';
import type { Workspace } from '../hooks/useWorkspace';
import { CurrentValue } from './CurrentValue';
import { DraftDetails } from './DraftDetails';
import { Notifications } from './Notifications';
import { DraftHistory } from './DraftHistory';
import { DiscardConfirmation } from './DiscardConfirmation';

export function ConfigDrawer({ workspace }: { workspace: Workspace }) {
  const {
    selected,
    current,
    draft,
    drafts,
    editor,
    busy,
    closeConfig,
    setDraft,
    run,
    reload,
    discardChanges,
  } = workspace;
  const dialog = useRef<HTMLDialogElement>(null);
  const content = useRef<HTMLDivElement>(null);
  const pressedOutside = useRef(false);
  const [historyDetail, setHistoryDetail] = useState(false);
  const [tab, setTab] = useState<'value' | 'history'>('value');
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null;
    dialog.current?.showModal();
    const old = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    return () => {
      document.body.style.overflow = old;
      previous?.focus();
    };
  }, []);
  useEffect(() => {
    if (editor) setTab('value');
  }, [editor]);
  useEffect(() => {
    content.current?.scrollTo({ top: 0 });
  }, [historyDetail, tab, editor]);
  function close() {
    if (!busy) discardChanges(closeConfig);
  }
  function outsidePanel(event: {
    currentTarget: HTMLDialogElement;
    target: EventTarget | null;
    clientX: number;
    clientY: number;
  }) {
    if (event.target !== event.currentTarget) return false;
    const rect = event.currentTarget.getBoundingClientRect();
    return (
      event.clientX < rect.left ||
      event.clientX >= rect.right ||
      event.clientY < rect.top ||
      event.clientY >= rect.bottom
    );
  }
  return (
    <dialog
      ref={dialog}
      className="drawer"
      aria-labelledby="config-title"
      onPointerDown={(event) => {
        pressedOutside.current = event.button === 0 && outsidePanel(event);
      }}
      onClick={(event) => {
        const shouldClose = pressedOutside.current && outsidePanel(event);
        pressedOutside.current = false;
        if (shouldClose) close();
      }}
      onCancel={(event) => {
        event.preventDefault();
        close();
      }}
    >
      <DiscardConfirmation workspace={workspace} />
      <Notifications workspace={workspace} />
      <div className="drawer-heading">
        <div>
          <p className="eyebrow">КОНФИГ</p>
          <h1 id="config-title">{workspace.creating ? 'Новый конфиг' : selected}</h1>
          {current && (
            <p>
              {current.value === null
                ? current.version === 0
                  ? 'Ещё не опубликован'
                  : 'Удалён'
                : `Версия ${current.version} · Применяется без перезапуска`}
            </p>
          )}
        </div>
        <button aria-label="Закрыть конфиг" disabled={busy} onClick={close}>
          ✕
        </button>
      </div>
      <div className="drawer-toolbar">
        <div role="tablist" aria-label="Разделы конфига">
          <button
            role="tab"
            aria-selected={tab === 'value'}
            aria-controls="config-value"
            id="value-tab"
            onClick={() => {
              setDraft(drafts.find((item) => item.id === current?.active_draft_id));
              setTab('value');
            }}
          >
            Значение
          </button>
          <button
            role="tab"
            aria-selected={tab === 'history'}
            aria-controls="config-history"
            id="history-tab"
            disabled={editor}
            onClick={() => {
              setTab('history');
              setHistoryDetail(false);
            }}
          >
            История
          </button>
        </div>
        <button disabled={busy || editor} onClick={() => run(reload)}>
          Обновить
        </button>
      </div>
      <div className="drawer-content" ref={content}>
        {!current ? (
          <p>Загружаем конфиг…</p>
        ) : (
          <>
            <div
              role="tabpanel"
              id="config-value"
              aria-labelledby="value-tab"
              hidden={tab !== 'value'}
            >
              <CurrentValue workspace={workspace} />
              {!editor && draft?.status === 'draft' && <DraftDetails workspace={workspace} />}
            </div>
            <div
              role="tabpanel"
              id="config-history"
              aria-labelledby="history-tab"
              hidden={tab !== 'history'}
            >
              {historyDetail && draft ? (
                <>
                  <button className="back-button" onClick={() => setHistoryDetail(false)}>
                    ← Назад к истории
                  </button>
                  <DraftDetails workspace={workspace} />
                </>
              ) : (
                <DraftHistory
                  workspace={workspace}
                  onSelect={(item) => {
                    setDraft(item);
                    setHistoryDetail(true);
                  }}
                />
              )}
            </div>
          </>
        )}
      </div>
    </dialog>
  );
}
