import { useEffect, useId, useRef, useState, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { modes, ownership, transportationTypes, type Transportation } from './api';

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
            <button aria-label="Закрыть" onClick={close}>
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
  const [pending, setPending] = useState<(() => void) | null>(null);
  function confirmDiscard(dirty: boolean, discard: () => void) {
    if (dirty) setPending(() => discard);
    else discard();
  }
  const confirmation = pending && (
    <Dialog
      className="discard-dialog"
      title="Не сохранять изменения?"
      onClose={() => setPending(null)}
    >
      <p className="confirmation-copy">При выходе изменения в этой форме будут потеряны.</p>
      <div className="confirmation-actions">
        <button type="button" data-autofocus onClick={() => setPending(null)}>
          Остаться
        </button>
        <button
          type="button"
          className="danger"
          onClick={() => {
            setPending(null);
            pending();
          }}
        >
          Не сохранять
        </button>
      </div>
    </Dialog>
  );
  return { confirmDiscard, confirmation };
}

export function CardNavigation({
  children,
  onRefresh,
  disabled = false,
}: {
  children: ReactNode;
  onRefresh: () => Promise<unknown>;
  disabled?: boolean;
}) {
  const [refreshing, setRefreshing] = useState(false);
  return (
    <div className="card-navigation">
      <div className="tabs">{children}</div>
      <button
        type="button"
        className="card-refresh"
        disabled={disabled || refreshing}
        onClick={async () => {
          setRefreshing(true);
          try {
            await onRefresh();
          } finally {
            setRefreshing(false);
          }
        }}
      >
        <span aria-hidden="true">↻</span> {refreshing ? 'Обновляем…' : 'Обновить'}
      </button>
    </div>
  );
}

export function TransportationTable({ items }: { items: Transportation[] }) {
  return (
    <div className="table-scroll">
      <table className="transportation-table">
        <thead>
          <tr>
            <th>Перевозка</th>
            <th>Контейнер</th>
            <th>Маршрут</th>
          </tr>
        </thead>
        <tbody>
          {items.map((item) => (
            <tr
              key={item.id}
              className="clickable"
              onClick={() => {
                location.hash = `/transportations/${item.id}`;
              }}
            >
              <td className="transportation-number-cell">
                <a
                  className="link"
                  href={`#/transportations/${item.id}`}
                  onClick={(event) => event.stopPropagation()}
                >
                  {item.number}
                </a>
                <span className="registry-secondary">{transportationTypes[item.type]}</span>
                {item.hazardous && <span className="registry-hazard">Опасный груз</span>}
              </td>
              <td className="transportation-container-cell">
                {item.container_type_code ? (
                  <strong>{item.container_type_code}</strong>
                ) : (
                  <span className="registry-muted">—</span>
                )}
                {item.container_ownership && (
                  <span className="registry-ownership">{ownership[item.container_ownership]}</span>
                )}
              </td>
              <td>
                {item.route_segments.length ? (
                  <div className="registry-route">
                    {item.route_segments.slice(0, 2).map((stage) => (
                      <div className="registry-route-stage" key={stage.id}>
                        <span className="registry-route-mode">{modes[stage.mode]}</span>
                        {(stage.origin.name || stage.destination.name) && (
                          <span>
                            {stage.origin.name || 'Не указан'} →{' '}
                            {stage.destination.name || 'Не указан'}
                          </span>
                        )}
                      </div>
                    ))}
                    {item.route_segments.length > 2 && (
                      <span className="registry-secondary">
                        Ещё участков: {item.route_segments.length - 2}
                      </span>
                    )}
                  </div>
                ) : (
                  <span className="registry-muted">Не задан</span>
                )}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
