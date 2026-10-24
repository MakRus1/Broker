import { useRef, useState } from 'react';
import { moveWorkspace, type WorkspaceTab } from './workspace';

export function WorkspaceTabs({
  tabs,
  active,
  onMove,
  onClose,
}: {
  tabs: WorkspaceTab[];
  active: string;
  onMove: (source: string, target: string, side: 'before' | 'after') => void;
  onClose: (id: string) => void;
}) {
  const dragging = useRef<string | null>(null);
  const [source, setSource] = useState<string | null>(null);
  const [drop, setDrop] = useState<{ id: string; side: 'before' | 'after' } | null>(null);
  const [announcement, setAnnouncement] = useState('');
  function finish() {
    dragging.current = null;
    setSource(null);
    setDrop(null);
  }
  function move(id: string, target: string, side: 'before' | 'after') {
    const next = moveWorkspace(tabs, id, target, side);
    onMove(id, target, side);
    const index = next.findIndex((tab) => tab.id === id);
    setAnnouncement(`Вкладка «${next[index].title}»: позиция ${index + 1} из ${next.length}`);
  }
  return (
    <>
      <div className="workspace-tabs" role="tablist" aria-label="Открытые формы">
        {tabs.map((entry, index) => (
          <div
            key={entry.id}
            className={`workspace-tab${active === entry.id ? ' selected' : ''}${source === entry.id ? ' dragging' : ''}`}
            data-drop-side={drop?.id === entry.id ? drop.side : undefined}
            draggable
            onDragStart={(event) => {
              if ((event.target as HTMLElement).closest('.workspace-close')) {
                event.preventDefault();
                return;
              }
              dragging.current = entry.id;
              setSource(entry.id);
              event.dataTransfer.effectAllowed = 'move';
              event.dataTransfer.setData('text/plain', entry.id);
            }}
            onDragOver={(event) => {
              if (!dragging.current || dragging.current === entry.id) return;
              event.preventDefault();
              event.dataTransfer.dropEffect = 'move';
              const rect = event.currentTarget.getBoundingClientRect();
              setDrop({
                id: entry.id,
                side: event.clientX < rect.left + rect.width / 2 ? 'before' : 'after',
              });
            }}
            onDragLeave={(event) => {
              if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setDrop(null);
            }}
            onDrop={(event) => {
              if (!dragging.current) return;
              event.preventDefault();
              const rect = event.currentTarget.getBoundingClientRect();
              move(
                dragging.current,
                entry.id,
                event.clientX < rect.left + rect.width / 2 ? 'before' : 'after',
              );
              finish();
            }}
            onDragEnd={finish}
          >
            <button
              type="button"
              role="tab"
              aria-selected={active === entry.id}
              title="Перетащите вкладку для изменения порядка. С клавиатуры: Alt + ← / →"
              aria-keyshortcuts="Alt+ArrowLeft Alt+ArrowRight"
              onKeyDown={(event) => {
                if (!event.altKey || (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight'))
                  return;
                event.preventDefault();
                const target = tabs[index + (event.key === 'ArrowLeft' ? -1 : 1)];
                if (target)
                  move(entry.id, target.id, event.key === 'ArrowLeft' ? 'before' : 'after');
              }}
              onClick={() => {
                location.hash = entry.route;
              }}
            >
              {entry.title}
            </button>
            <button
              type="button"
              className="workspace-close"
              draggable={false}
              aria-label={`Закрыть вкладку ${entry.title}`}
              onClick={() => onClose(entry.id)}
            >
              ×
            </button>
          </div>
        ))}
      </div>
      <span className="visually-hidden" role="status">
        {announcement}
      </span>
    </>
  );
}
