import { useEffect, useId, useRef, useState, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { modes, ownership, type Transportation } from './api';

export function Dialog({
  title,
  onClose,
  children,
}: {
  title: string;
  onClose: () => void;
  children: ReactNode;
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
    <Dialog title="Отменить изменения?" onClose={() => setPending(null)}>
      <p className="confirmation-copy">
        Введённые данные не будут сохранены. Можно вернуться к редактированию.
      </p>
      <div className="confirmation-actions">
        <button type="button" data-autofocus onClick={() => setPending(null)}>
          Продолжить редактирование
        </button>
        <button
          type="button"
          className="danger"
          onClick={() => {
            setPending(null);
            pending();
          }}
        >
          Отменить изменения
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
      <table>
        <thead>
          <tr>
            <th>Перевозка</th>
            <th>Контейнер</th>
            <th>Принадлежность</th>
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
              <td>
                <a
                  className="link"
                  href={`#/transportations/${item.id}`}
                  onClick={(event) => event.stopPropagation()}
                >
                  {item.number}
                </a>
              </td>
              <td>{item.container_type_code}</td>
              <td>{ownership[item.container_ownership]}</td>
              <td>
                {item.route_segments.length
                  ? item.route_segments
                      .map(
                        (route_segment) =>
                          `${modes[route_segment.mode]}: ${route_segment.origin.name} → ${route_segment.destination.name}`,
                      )
                      .join(' · ')
                  : 'Маршрут ещё не задан'}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
