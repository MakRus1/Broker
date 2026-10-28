import { useEffect, useId, useRef, useState } from 'react';
import { createPortal } from 'react-dom';

export function Chevron() {
  return (
    <svg
      width="16"
      height="16"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="m6 9 6 6 6-6" />
    </svg>
  );
}

export function Select({
  label,
  value,
  options,
  onChange,
  disabled = false,
}: {
  label: string;
  value: string;
  options: { value: string; label: string }[];
  onChange: (value: string) => void;
  disabled?: boolean;
}) {
  const id = useId();
  const trigger = useRef<HTMLButtonElement>(null);
  const [position, setPosition] = useState({
    left: 0,
    top: 0,
    width: 0,
    maxHeight: 240,
    transform: '',
  });
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(0);
  useEffect(() => {
    if (!open) return;
    const update = () => {
      const rect = trigger.current?.getBoundingClientRect();
      if (!rect) return;
      const below = window.innerHeight - rect.bottom - 12;
      const above = rect.top - 12;
      const upwards = below < 180 && above > below;
      const height = Math.min(240, upwards ? above : below);
      setPosition({
        left: rect.left,
        width: rect.width,
        top: upwards ? rect.top - 5 : rect.bottom + 5,
        transform: upwards ? 'translateY(-100%)' : '',
        maxHeight: height,
      });
    };
    update();
    window.addEventListener('resize', update);
    window.addEventListener('scroll', update, true);
    return () => {
      window.removeEventListener('resize', update);
      window.removeEventListener('scroll', update, true);
    };
  }, [open]);
  function choose(next: string) {
    onChange(next);
    setOpen(false);
  }
  return (
    <div
      className="app-select"
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) setOpen(false);
      }}
    >
      <span className="field-label" id={`${id}-label`}>
        {label}
      </span>
      <button
        ref={trigger}
        type="button"
        className="select-trigger"
        role="combobox"
        aria-labelledby={`${id}-label`}
        aria-expanded={open}
        aria-controls={open ? id : undefined}
        aria-activedescendant={open ? `${id}-${active}` : undefined}
        disabled={disabled}
        onClick={() => {
          setActive(
            Math.max(
              0,
              options.findIndex((item) => item.value === value),
            ),
          );
          setOpen((old) => !old);
        }}
        onKeyDown={(event) => {
          if (event.key === 'Escape') {
            if (open) event.stopPropagation();
            event.preventDefault();
            setOpen(false);
          }
          if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault();
            setOpen(true);
            setActive(
              (old) =>
                (old + (event.key === 'ArrowDown' ? 1 : -1) + options.length) % options.length,
            );
          }
          if (event.key === 'Enter' && open) {
            event.preventDefault();
            choose(options[active].value);
          }
        }}
      >
        <span>{options.find((item) => item.value === value)?.label ?? 'Не выбрано'}</span>
        <Chevron />
      </button>
      {open &&
        createPortal(
          <div
            className="counterparty-options select-popup"
            style={{ position: 'fixed', ...position, zIndex: 1000 }}
            role="listbox"
            id={id}
            aria-labelledby={`${id}-label`}
          >
            {options.map((item, index) => (
              <button
                type="button"
                role="option"
                id={`${id}-${index}`}
                key={item.value}
                aria-selected={item.value === value}
                className={index === active ? 'active' : ''}
                onMouseDown={(event) => event.preventDefault()}
                onClick={() => choose(item.value)}
              >
                {item.label}
              </button>
            ))}
          </div>,
          // Модальный dialog делает содержимое body вне себя инертным.
          trigger.current?.closest('dialog') ?? document.body,
        )}
    </div>
  );
}
