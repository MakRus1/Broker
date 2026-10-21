import { useEffect, useId, useRef, type ReactNode } from 'react';
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
  useEffect(() => {
    const node = ref.current!;
    node.showModal();
    node.querySelector<HTMLElement>('input,textarea,select')?.focus();
    return () => node.close();
  }, []);
  function close() {
    if (ref.current?.querySelector('form[data-busy="true"]')) return;
    if (
      !ref.current?.querySelector('form[data-dirty="true"]') ||
      confirm('Несохранённые изменения будут потеряны. Закрыть?')
    )
      onClose();
  }
  return createPortal(
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
        if (startedOnBackdrop.current && event.target === event.currentTarget && outside) close();
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
