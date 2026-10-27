import { useEffect, useId, useRef, useState } from 'react';

export function Select({
  label,
  value,
  options,
  onChange,
}: {
  label: string;
  value: string;
  options: { value: string; label: string }[];
  onChange: (value: string) => void;
}) {
  const id = useId();
  const root = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  const [active, setActive] = useState(0);
  const selected = Math.max(
    0,
    options.findIndex((option) => option.value === value),
  );
  useEffect(() => {
    if (!open) return;
    const outside = (event: PointerEvent) => {
      if (!root.current?.contains(event.target as Node)) setOpen(false);
    };
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, [open]);
  const choose = (index: number) => {
    if (options[index]) onChange(options[index].value);
    setOpen(false);
  };
  return (
    <div
      className="app-select admin-select"
      ref={root}
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
        onClick={() => {
          setActive(selected);
          setOpen(!open);
        }}
        onKeyDown={(event) => {
          if (event.key === 'Escape' && open) {
            event.preventDefault();
            event.stopPropagation();
            setOpen(false);
          } else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault();
            setActive(
              open
                ? (active + (event.key === 'ArrowDown' ? 1 : -1) + options.length) % options.length
                : selected,
            );
            setOpen(true);
          } else if (open && (event.key === 'Home' || event.key === 'End')) {
            event.preventDefault();
            setActive(event.key === 'Home' ? 0 : options.length - 1);
          } else if (event.key === 'Enter' || event.key === ' ') {
            event.preventDefault();
            if (open) choose(active);
            else {
              setActive(selected);
              setOpen(true);
            }
          }
        }}
      >
        <span>{options[selected]?.label ?? 'Не выбрано'}</span>
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
      </button>
      {open && (
        <div className="select-options" role="listbox" id={id} aria-labelledby={`${id}-label`}>
          {options.map((option, index) => (
            <button
              type="button"
              role="option"
              tabIndex={-1}
              id={`${id}-${index}`}
              key={option.value}
              aria-selected={option.value === value}
              className={index === active ? 'active' : ''}
              onMouseDown={(event) => event.preventDefault()}
              onClick={() => choose(index)}
            >
              {option.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
