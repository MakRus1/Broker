import { useEffect } from 'react';
export function RegistrySelectionActions({
  count,
  problem,
  actions,
  onClear,
  menu,
  onCloseMenu,
}: {
  count: number;
  problem?: string;
  actions: { label: string; onClick: () => void }[];
  onClear: () => void;
  menu?: { x: number; y: number };
  onCloseMenu: () => void;
}) {
  useEffect(() => {
    if (!menu) return;
    const close = () => onCloseMenu();
    const key = (event: KeyboardEvent) => {
      if (event.key === 'Escape') close();
    };
    window.addEventListener('pointerdown', close);
    window.addEventListener('scroll', close, true);
    window.addEventListener('keydown', key);
    return () => {
      window.removeEventListener('pointerdown', close);
      window.removeEventListener('scroll', close, true);
      window.removeEventListener('keydown', key);
    };
  }, [menu, onCloseMenu]);
  if (!count) return null;
  return (
    <>
      <div className="bulk-services-toolbar" role="region" aria-label="Групповые действия">
        <strong>Выбрано: {count}</strong>
        {actions.map((action, index) => (
          <button
            key={action.label}
            className={index === 0 ? 'primary' : undefined}
            disabled={!!problem}
            onClick={action.onClick}
          >
            {action.label}
          </button>
        ))}
        <button onClick={onClear}>Снять выделение</button>
        {problem && <span role="status">{problem}</span>}
      </div>
      {menu && (
        <div
          className="bulk-services-menu"
          role="menu"
          style={{ left: menu.x, top: menu.y }}
          onPointerDown={(event) => event.stopPropagation()}
        >
          <span>Выбрано: {count}</span>
          {actions.map((action) => (
            <button
              role="menuitem"
              key={action.label}
              disabled={!!problem}
              onClick={action.onClick}
            >
              {action.label}
            </button>
          ))}
          {problem && <small>{problem}</small>}
        </div>
      )}
    </>
  );
}
