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
      onClick={(event) => {
        if (event.target === ref.current) close();
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
            <tr key={item.id}>
              <td>
                <strong>{item.number}</strong>
              </td>
              <td>{item.container_type_code}</td>
              <td>{ownership[item.container_ownership]}</td>
              <td>
                {item.legs.length
                  ? item.legs
                      .map(
                        (leg) => `${modes[leg.mode]}: ${leg.origin.name} → ${leg.destination.name}`,
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
