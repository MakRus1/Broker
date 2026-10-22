import { useId, useState } from 'react';

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
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(0);
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
      {open && (
        <div
          className="counterparty-options"
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
        </div>
      )}
    </div>
  );
}
