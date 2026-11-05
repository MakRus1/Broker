import { useEffect, useId, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { useLocalization } from '../../features/localization/Localization';
import { Chevron } from './Select';
import { ClearSelectionButton } from './ClearSelectionButton';
import './lookupField.css';

export type LookupPage<T> = { items: T[]; hasMore?: boolean };

/** Shared interaction for remote directories; domain wrappers own loading and validation rules. */
export function LookupField<T extends { id: string }>({
  label,
  name,
  value,
  selectedLabel,
  placeholder,
  load,
  itemLabel,
  itemDescription,
  onChange,
  disabled = false,
  required = false,
  validationMessage = '',
  error = '',
}: {
  label: string;
  name?: string;
  value: string;
  selectedLabel: string;
  placeholder: string;
  load: (query: string, signal: AbortSignal) => Promise<LookupPage<T>>;
  itemLabel: (item: T) => string;
  itemDescription?: (item: T) => string;
  onChange: (item: T | null) => void;
  disabled?: boolean;
  required?: boolean;
  validationMessage?: string;
  error?: string;
}) {
  const { translate } = useLocalization();
  const id = useId();
  const root = useRef<HTMLDivElement>(null);
  const input = useRef<HTMLInputElement>(null);
  const popup = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [page, setPage] = useState<LookupPage<T>>({ items: [] });
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [retry, setRetry] = useState(0);
  const [active, setActive] = useState(-1);
  const [position, setPosition] = useState({
    left: 0,
    top: 0,
    width: 0,
    maxHeight: 260,
    transform: '',
  });
  const visible = open && !disabled;
  useEffect(() => {
    if (disabled) {
      setOpen(false);
      setQuery('');
    }
  }, [disabled]);
  useEffect(() => {
    input.current?.setCustomValidity(required && !value ? validationMessage : '');
  }, [required, value, validationMessage]);
  useEffect(() => {
    if (!visible) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setPage({ items: [] });
    setActive(-1);
    const timer = window.setTimeout(
      () => {
        void load(query.trim(), controller.signal)
          .then((result) => {
            if (!controller.signal.aborted) setPage(result);
          })
          .catch((cause: unknown) => {
            if (!controller.signal.aborted)
              setFailure(cause instanceof Error ? cause.message : translate('filter.loadError'));
          })
          .finally(() => {
            if (!controller.signal.aborted) setLoading(false);
          });
      },
      query.trim() ? 250 : 0,
    );
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [visible, query, load, retry, translate]);
  useEffect(() => {
    if (!visible) return;
    const update = () => {
      const rect = input.current?.getBoundingClientRect();
      if (!rect) return;
      const below = window.innerHeight - rect.bottom - 12;
      const above = rect.top - 12;
      const upwards = below < 180 && above > below;
      setPosition({
        left: rect.left,
        top: upwards ? rect.top - 5 : rect.bottom + 5,
        width: rect.width,
        maxHeight: Math.max(80, Math.min(300, upwards ? above : below)),
        transform: upwards ? 'translateY(-100%)' : '',
      });
    };
    const dismiss = (event: PointerEvent) => {
      if (
        event.target instanceof Node &&
        !root.current?.contains(event.target) &&
        !popup.current?.contains(event.target)
      )
        setOpen(false);
    };
    update();
    document.addEventListener('pointerdown', dismiss);
    window.addEventListener('scroll', update, true);
    window.addEventListener('resize', update);
    return () => {
      document.removeEventListener('pointerdown', dismiss);
      window.removeEventListener('scroll', update, true);
      window.removeEventListener('resize', update);
    };
  }, [visible]);
  useEffect(() => {
    if (active >= 0)
      document.getElementById(`${id}-option-${active}`)?.scrollIntoView({ block: 'nearest' });
  }, [active, id]);
  function close() {
    setOpen(false);
    setQuery('');
    setActive(-1);
  }
  function choose(item: T) {
    onChange(item);
    close();
  }
  return (
    <div
      className="lookup-field"
      ref={root}
      onBlur={(event) => {
        if (
          !event.currentTarget.contains(event.relatedTarget) &&
          !popup.current?.contains(event.relatedTarget)
        )
          close();
      }}
    >
      <label htmlFor={id}>{label}</label>
      <div className="lookup-field-control" data-clearable={!!value}>
        <input
          ref={input}
          id={id}
          name={name}
          maxLength={200}
          role="combobox"
          autoComplete="off"
          disabled={disabled}
          required={required && !value}
          aria-invalid={!!error || undefined}
          aria-describedby={error ? `${id}-error` : undefined}
          aria-expanded={visible}
          aria-controls={visible ? `${id}-list` : undefined}
          aria-autocomplete="list"
          aria-activedescendant={
            visible && !loading && page.items[active] ? `${id}-option-${active}` : undefined
          }
          value={visible ? query : selectedLabel}
          placeholder={placeholder}
          onFocus={() => {
            setQuery('');
            setOpen(true);
          }}
          onChange={(event) => {
            setQuery(event.target.value);
            setOpen(true);
          }}
          onKeyDown={(event) => {
            if (event.key === 'Escape' && visible) {
              event.preventDefault();
              event.stopPropagation();
              close();
            }
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
              event.preventDefault();
              setOpen(true);
              setActive((old) =>
                !page.items.length || loading
                  ? -1
                  : old < 0
                    ? event.key === 'ArrowDown'
                      ? 0
                      : page.items.length - 1
                    : (old + (event.key === 'ArrowDown' ? 1 : -1) + page.items.length) %
                      page.items.length,
              );
            }
            if (event.key === 'Enter' && visible) {
              event.preventDefault();
              if (!loading && page.items[active]) choose(page.items[active]);
            }
          }}
        />
        {value && (
          <ClearSelectionButton
            label={label}
            disabled={disabled}
            onClick={() => {
              input.current?.focus();
              onChange(null);
              close();
            }}
          />
        )}
        <button
          type="button"
          className="lookup-field-toggle"
          disabled={disabled}
          aria-label={translate('lookup.openDirectory', { label })}
          onMouseDown={(event) => event.preventDefault()}
          onClick={() => {
            if (visible) close();
            else {
              input.current?.focus();
              setQuery('');
              setOpen(true);
            }
          }}
        >
          <Chevron />
        </button>
      </div>
      {error && (
        <small id={`${id}-error`} className="field-error" role="alert">
          {error}
        </small>
      )}
      {visible &&
        createPortal(
          <div
            ref={popup}
            className="lookup-field-popup"
            style={{ position: 'fixed', ...position, zIndex: 1000 }}
          >
            {loading && <p role="status">{translate('lookup.directorySearching')}</p>}
            {failure && (
              <div className="lookup-field-failure" role="alert">
                <p>{failure}</p>
                <button type="button" onClick={() => setRetry((old) => old + 1)}>
                  {translate('ui.retryLoad')}
                </button>
              </div>
            )}
            {!loading && !failure && !page.items.length && (
              <p role="status">{translate('ui.noMatches')}</p>
            )}
            <div role="listbox" id={`${id}-list`} aria-label={label} aria-busy={loading}>
              {!loading &&
                !failure &&
                page.items.map((item, index) => (
                  <button
                    type="button"
                    role="option"
                    id={`${id}-option-${index}`}
                    key={item.id}
                    aria-selected={item.id === value}
                    className={active === index ? 'active' : ''}
                    onMouseDown={(event) => event.preventDefault()}
                    onClick={() => choose(item)}
                  >
                    <span>{itemLabel(item)}</span>
                    {itemDescription?.(item) && <small>{itemDescription(item)}</small>}
                  </button>
                ))}
            </div>
            {!loading && !failure && page.hasMore && <p>{translate('lookup.moreResults')}</p>}
          </div>,
          input.current?.closest('dialog') ?? document.body,
        )}
    </div>
  );
}
