import { useEffect, useId, useRef, useState, type KeyboardEvent } from 'react';
import { useLocalization } from '../../features/localization/Localization';
import { Chevron } from './Select';
import './multiSelect.css';

export function MultiSelect({
  label,
  values,
  options,
  onChange,
  disabled = false,
}: {
  label: string;
  values: string[];
  options: { value: string; label: string }[];
  onChange: (values: string[]) => void;
  disabled?: boolean;
}) {
  const { translate } = useLocalization();
  const id = useId();
  const root = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
  const list = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  useEffect(() => {
    if (!open) return;
    const outside = (event: PointerEvent) => {
      if (!root.current?.contains(event.target as Node)) setOpen(false);
    };
    list.current
      ?.querySelector<HTMLButtonElement>('[aria-selected="true"], [role="option"]')
      ?.focus();
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, [open]);
  function toggle(value: string) {
    onChange(values.includes(value) ? values.filter((item) => item !== value) : [...values, value]);
  }
  function navigate(event: KeyboardEvent<HTMLDivElement>) {
    const items = [...(list.current?.querySelectorAll<HTMLButtonElement>('[role="option"]') ?? [])];
    const index = items.indexOf(document.activeElement as HTMLButtonElement);
    const next =
      event.key === 'ArrowDown'
        ? (index + 1) % items.length
        : event.key === 'ArrowUp'
          ? (index - 1 + items.length) % items.length
          : event.key === 'Home'
            ? 0
            : event.key === 'End'
              ? items.length - 1
              : undefined;
    if (next !== undefined) {
      event.preventDefault();
      items[next]?.focus();
    }
  }
  return (
    <div
      className="multi-select"
      ref={root}
      role="group"
      aria-label={label}
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) setOpen(false);
      }}
      onKeyDown={(event) => {
        if (event.key === 'Escape' && open) {
          event.preventDefault();
          event.stopPropagation();
          setOpen(false);
          trigger.current?.focus();
        }
      }}
    >
      <div className="multi-select-control">
        {options
          .filter((item) => values.includes(item.value))
          .map((item) => (
            <span className="multi-select-tag" key={item.value}>
              {item.label}
              <button
                type="button"
                disabled={disabled}
                aria-label={translate('select.removeValue', { value: item.label })}
                onClick={() => toggle(item.value)}
              >
                <svg width="12" height="12" viewBox="0 0 16 16" aria-hidden="true">
                  <path
                    d="m4 4 8 8M12 4l-8 8"
                    fill="none"
                    stroke="currentColor"
                    strokeWidth="1.5"
                    strokeLinecap="round"
                  />
                </svg>
              </button>
            </span>
          ))}
        <button
          type="button"
          className="multi-select-trigger"
          ref={trigger}
          disabled={disabled}
          aria-label={label}
          aria-expanded={open}
          aria-haspopup="listbox"
          aria-controls={open ? id : undefined}
          onClick={() => setOpen((value) => !value)}
          onKeyDown={(event) => {
            if (event.key === 'ArrowDown') {
              event.preventDefault();
              setOpen(true);
            }
          }}
        >
          <span>{values.length ? translate('select.changeValues') : label}</span>
          <Chevron />
        </button>
      </div>
      {open && (
        <div
          className="multi-select-options"
          ref={list}
          id={id}
          role="listbox"
          aria-label={label}
          aria-multiselectable="true"
          onKeyDown={navigate}
        >
          {options.map((item) => (
            <button
              type="button"
              role="option"
              key={item.value}
              aria-selected={values.includes(item.value)}
              onClick={() => toggle(item.value)}
            >
              <span>{item.label}</span>
              {values.includes(item.value) && (
                <svg width="16" height="16" viewBox="0 0 20 20" aria-hidden="true">
                  <path
                    d="m4 10 4 4 8-8"
                    fill="none"
                    stroke="currentColor"
                    strokeWidth="1.8"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                  />
                </svg>
              )}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
