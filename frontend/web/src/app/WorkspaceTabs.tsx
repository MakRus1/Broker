import { useLocalization } from '../features/localization/Localization';
import { useEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { moveWorkspace, workspaceTitle, type WorkspaceTab } from './workspace';

function Chevron({ direction }: { direction: 'left' | 'right' | 'down' }) {
  return (
    <svg className={`workspace-chevron ${direction}`} viewBox="0 0 24 24" aria-hidden="true">
      <path d="m9 5 7 7-7 7" />
    </svg>
  );
}

export function WorkspaceTabs({
  tabs,
  active,
  onMove,
  onClose,
  onCloseAll,
}: {
  tabs: WorkspaceTab[];
  active: string;
  onMove: (source: string, target: string, side: 'before' | 'after') => void;
  onClose: (id: string) => void;
  onCloseAll: () => void;
}) {
  const { translate, locale } = useLocalization();
  const strip = useRef<HTMLDivElement>(null);
  const menu = useRef<HTMLDivElement>(null);
  const menuButton = useRef<HTMLButtonElement>(null);
  const [listOpen, setListOpen] = useState(false);
  const [busy, setBusy] = useState(false);
  const [query, setQuery] = useState('');
  const [edges, setEdges] = useState({ before: false, after: false });
  const dragging = useRef<string | null>(null);
  const contextNode = useRef<HTMLDivElement>(null);
  const contextOrigin = useRef<HTMLElement | null>(null);
  const [context, setContext] = useState<{ x: number; y: number } | null>(null);
  function openContext(x: number, y: number, origin: HTMLElement) {
    setListOpen(false);
    setBusy(Boolean(document.querySelector('.workspace-content form[data-busy="true"]')));
    contextOrigin.current = origin.querySelector<HTMLButtonElement>('button[role="tab"]') ?? origin;
    setContext({
      x: Math.max(8, Math.min(x, window.innerWidth - 232)),
      y: Math.max(8, Math.min(y, window.innerHeight - 56)),
    });
  }
  useEffect(() => {
    if (!context) return;
    const element = contextNode.current;
    (element?.querySelector<HTMLButtonElement>('button:not(:disabled)') ?? element)?.focus({
      preventScroll: true,
    });
    function outside(event: PointerEvent) {
      if (!contextNode.current?.contains(event.target as Node)) setContext(null);
    }
    function key(event: KeyboardEvent) {
      if (event.key === 'Escape') {
        event.preventDefault();
        setContext(null);
        contextOrigin.current?.focus({ preventScroll: true });
      } else if (event.key === 'Tab') setContext(null);
    }
    const close = () => setContext(null);
    document.addEventListener('pointerdown', outside);
    window.addEventListener('scroll', close, true);
    window.addEventListener('resize', close);
    document.addEventListener('keydown', key);
    return () => {
      document.removeEventListener('pointerdown', outside);
      window.removeEventListener('scroll', close, true);
      window.removeEventListener('resize', close);
      document.removeEventListener('keydown', key);
    };
  }, [context]);
  useEffect(() => {
    const element = strip.current;
    if (!element) return;
    const update = () => {
      setEdges({
        before: element.scrollLeft > 1,
        after: element.scrollLeft + element.clientWidth < element.scrollWidth - 1,
      });
    };
    const revealSelected = () => {
      const selected = element.querySelector<HTMLElement>('.workspace-tab.selected');
      if (selected) {
        const bounds = element.getBoundingClientRect();
        const target = selected.getBoundingClientRect();
        if (target.left < bounds.left) element.scrollLeft -= bounds.left - target.left;
        else if (target.right > bounds.right) element.scrollLeft += target.right - bounds.right;
      }
    };
    revealSelected();
    update();
    element.addEventListener('scroll', update, { passive: true });
    const observer = new ResizeObserver(() => {
      revealSelected();
      update();
    });
    observer.observe(element);
    return () => {
      observer.disconnect();
      element.removeEventListener('scroll', update);
    };
  }, [active, tabs]);
  useEffect(() => {
    const element = strip.current;
    if (!element) return;
    const wheel = (event: WheelEvent) => {
      if (
        event.ctrlKey ||
        Math.abs(event.deltaX) >= Math.abs(event.deltaY) ||
        element.scrollWidth <= element.clientWidth
      )
        return;
      event.preventDefault();
      element.scrollLeft += event.deltaY * (event.deltaMode === 1 ? 16 : 1);
    };
    element.addEventListener('wheel', wheel, { passive: false });
    return () => element.removeEventListener('wheel', wheel);
  }, [tabs.length]);
  useEffect(() => {
    if (!listOpen) return;
    function outside(event: PointerEvent) {
      if (!menu.current?.contains(event.target as Node)) setListOpen(false);
    }
    function escape(event: KeyboardEvent) {
      if (event.key === 'Escape') {
        setListOpen(false);
        menuButton.current?.focus();
      }
    }
    document.addEventListener('pointerdown', outside);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', outside);
      document.removeEventListener('keydown', escape);
    };
  }, [listOpen]);
  function scroll(direction: number) {
    const element = strip.current;
    if (element) element.scrollLeft += direction * Math.max(160, element.clientWidth * 0.7);
  }
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
    setAnnouncement(
      translate('tabs.position', {
        title: next[index].title,
        position: index + 1,
        count: next.length,
      }),
    );
  }
  if (!tabs.length) return null;
  return (
    <>
      <div className="workspace-tabbar">
        {(edges.before || edges.after) && (
          <button
            type="button"
            className="workspace-scroll"
            aria-label={translate('tabs.left')}
            disabled={!edges.before}
            onClick={() => scroll(-1)}
          >
            <Chevron direction="left" />
          </button>
        )}
        <div
          ref={strip}
          className="workspace-tabs"
          role="tablist"
          aria-label={translate('tabs.forms')}
          onDragOver={(event) => {
            if (!dragging.current) return;
            const element = event.currentTarget;
            const bounds = element.getBoundingClientRect();
            if (event.clientX < bounds.left + 40) element.scrollLeft -= 24;
            else if (event.clientX > bounds.right - 40) element.scrollLeft += 24;
          }}
        >
          {tabs.map((entry, index) => (
            <div
              key={entry.id}
              data-workspace-tab={entry.id}
              className={`workspace-tab${active === entry.id ? ' selected' : ''}${source === entry.id ? ' dragging' : ''}`}
              data-drop-side={drop?.id === entry.id ? drop.side : undefined}
              draggable
              onContextMenu={(event) => {
                event.preventDefault();
                openContext(event.clientX, event.clientY, event.currentTarget);
              }}
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
                if (!event.currentTarget.contains(event.relatedTarget as Node | null))
                  setDrop(null);
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
                title={translate('tabs.reorder', { title: entry.title })}
                aria-keyshortcuts="Alt+ArrowLeft Alt+ArrowRight"
                onKeyDown={(event) => {
                  if (event.key === 'ContextMenu' || (event.shiftKey && event.key === 'F10')) {
                    event.preventDefault();
                    const rect = event.currentTarget.getBoundingClientRect();
                    openContext(rect.left, rect.bottom, event.currentTarget);
                    return;
                  }
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
                aria-label={translate('tabs.close', { title: entry.title })}
                onClick={() => onClose(entry.id)}
              >
                ×
              </button>
            </div>
          ))}
          <div
            className="workspace-drop-end"
            aria-hidden="true"
            onDragOver={(event) => {
              const last = tabs.at(-1);
              if (!dragging.current || !last) return;
              event.preventDefault();
              event.dataTransfer.dropEffect = 'move';
              setDrop(dragging.current === last.id ? null : { id: last.id, side: 'after' });
            }}
            onDragLeave={(event) => {
              if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setDrop(null);
            }}
            onDrop={(event) => {
              const last = tabs.at(-1);
              if (!dragging.current || !last) return;
              event.preventDefault();
              move(dragging.current, last.id, 'after');
              finish();
            }}
          />
        </div>
        {(edges.before || edges.after) && (
          <button
            type="button"
            className="workspace-scroll"
            aria-label={translate('tabs.right')}
            disabled={!edges.after}
            onClick={() => scroll(1)}
          >
            <Chevron direction="right" />
          </button>
        )}
        <div className="workspace-list-control" ref={menu}>
          <button
            ref={menuButton}
            type="button"
            className="workspace-scroll"
            aria-label={`${translate('tabs.all')} (${tabs.length})`}
            title={translate('tabs.all')}
            aria-expanded={listOpen}
            aria-controls="workspace-open-list"
            onClick={() => {
              setQuery('');
              setContext(null);
              setBusy(Boolean(document.querySelector('.workspace-content form[data-busy="true"]')));
              setListOpen((old) => !old);
            }}
          >
            <Chevron direction="down" />
          </button>
          {listOpen && (
            <div
              id="workspace-open-list"
              className="workspace-open-list"
              role="navigation"
              aria-label={translate('tabs.all')}
            >
              <div className="workspace-list-heading">
                <strong>{translate('tabs.open', { count: tabs.length })}</strong>
                <button
                  type="button"
                  className="workspace-close-all"
                  disabled={busy}
                  title={busy ? translate('tabs.wait') : translate('tabs.closeAll')}
                  onClick={() => {
                    setListOpen(false);
                    onCloseAll();
                  }}
                >
                  {translate('tabs.closeAllShort')}
                </button>
              </div>
              {tabs.length > 8 && (
                <input
                  className="workspace-tab-search"
                  aria-label={translate('tabs.find')}
                  placeholder={translate('tabs.placeholder')}
                  value={query}
                  onChange={(event) => setQuery(event.target.value)}
                  autoFocus
                />
              )}
              <div className="workspace-list-items">
                {tabs
                  .filter((entry) =>
                    (entry.title + ' ' + workspaceTitle(entry.route, locale))
                      .toLocaleLowerCase()
                      .includes(query.trim().toLocaleLowerCase()),
                  )
                  .map((entry) => (
                    <button
                      key={entry.id}
                      type="button"
                      aria-current={active === entry.id ? 'page' : undefined}
                      onClick={() => {
                        location.hash = entry.route;
                        setListOpen(false);
                        strip.current
                          ?.querySelector<HTMLButtonElement>(
                            `[data-workspace-tab="${entry.id}"] button[role="tab"]`,
                          )
                          ?.focus({ preventScroll: true });
                      }}
                    >
                      <span className="workspace-list-title">
                        {entry.title}
                        {workspaceTitle(entry.route, locale) !== entry.title && (
                          <small>{workspaceTitle(entry.route, locale)}</small>
                        )}
                      </span>
                      {active === entry.id && <span aria-hidden="true">✓</span>}
                    </button>
                  ))}
                {!tabs.some((entry) =>
                  (entry.title + ' ' + workspaceTitle(entry.route, locale))
                    .toLocaleLowerCase()
                    .includes(query.trim().toLocaleLowerCase()),
                ) && <p className="workspace-search-empty">{translate('tabs.empty')}</p>}
              </div>
            </div>
          )}
        </div>
      </div>
      {context &&
        createPortal(
          <div
            ref={contextNode}
            className="workspace-context-menu"
            role="menu"
            aria-label={translate('tabs.actions')}
            tabIndex={-1}
            style={{ left: context.x, top: context.y }}
          >
            <button
              type="button"
              role="menuitem"
              disabled={busy}
              title={busy ? translate('tabs.wait') : undefined}
              onClick={() => {
                setContext(null);
                onCloseAll();
              }}
            >
              {translate('tabs.closeAll')}
            </button>
          </div>,
          document.body,
        )}
      <span className="visually-hidden" role="status">
        {announcement}
      </span>
    </>
  );
}
