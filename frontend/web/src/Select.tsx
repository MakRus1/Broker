import { useEffect, useId, useRef, useState, type KeyboardEvent } from 'react';
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
  searchable = false,
}: {
  label: string;
  value: string;
  options: { value: string; label: string }[];
  onChange: (value: string) => void;
  disabled?: boolean;
  searchable?: boolean;
}) {
  const id = useId();
  const popup = useRef<HTMLDivElement>(null);
  const search = useRef<HTMLInputElement>(null);
  const [query, setQuery] = useState('');
  const choices = options.filter(
    (item) =>
      !searchable ||
      `${item.value} ${item.label}`.toLocaleLowerCase().includes(query.trim().toLocaleLowerCase()),
  );
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
    if (searchable) search.current?.focus();
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
  }, [open, searchable]);
  function choose(next: string) {
    onChange(next);
    setOpen(false);
  }
  function keyDown(event: KeyboardEvent<HTMLInputElement | HTMLButtonElement>) {
    if (event.key === 'Escape') {
      if (open) event.stopPropagation();
      event.preventDefault();
      setOpen(false);
      trigger.current?.focus();
    }
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault();
      setOpen(true);
      if (choices.length)
        setActive(
          (old) => (old + (event.key === 'ArrowDown' ? 1 : -1) + choices.length) % choices.length,
        );
    }
    if (event.key === 'Enter' && open) {
      event.preventDefault();
      if (choices[active]) choose(choices[active].value);
    }
  }
  return (
    <div
      className="app-select"
      onBlur={(event) => {
        if (
          !event.currentTarget.contains(event.relatedTarget) &&
          !popup.current?.contains(event.relatedTarget)
        )
          setOpen(false);
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
          setQuery('');
          setOpen((old) => !old);
        }}
        onKeyDown={keyDown}
      >
        <span>{options.find((item) => item.value === value)?.label ?? 'Не выбрано'}</span>
        <Chevron />
      </button>
      {open &&
        createPortal(
          <div
            ref={popup}
            className="counterparty-options select-popup"
            style={{ position: 'fixed', ...position, zIndex: 1000 }}
            role="listbox"
            id={id}
            aria-labelledby={`${id}-label`}
          >
            {searchable && (
              <input
                className="select-search"
                ref={search}
                aria-label={`Поиск: ${label}`}
                placeholder="Код или название"
                value={query}
                onChange={(event) => {
                  setQuery(event.target.value);
                  setActive(0);
                }}
                onKeyDown={keyDown}
              />
            )}
            {searchable && !choices.length && <p className="hint">Совпадений нет</p>}
            {choices.map((item, index) => (
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
