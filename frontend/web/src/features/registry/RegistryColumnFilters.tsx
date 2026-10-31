import {
  useCallback,
  useEffect,
  useEffectEvent,
  useId,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { createPortal } from 'react-dom';
import { request } from '../../shared/api/http';
import { transportationTypes, modes } from '../routes/logisticsLabels';
import { type Session } from '../../shared/api/session';
import { counterpartyRoles } from '../counterparties/counterparties';
import { DateInput } from '../../shared/dates/DateInput';
import {
  filterRequest,
  replaceFilters,
  splitFilterValues,
  type ColumnFilter,
  type RegistryField,
  type RegistryValue,
} from './registryFilters';

export interface ColumnFilterController {
  fields: RegistryField[];
  filters: ColumnFilter[];
  sort: string;
  descending: boolean;
  failure: string;
  scope?: (field: RegistryField) => string;
  change: (filters: ColumnFilter[]) => void;
  order: (field: string, descending: boolean) => void;
  values: (
    field: RegistryField,
    search: string,
    after?: string,
    signal?: AbortSignal,
  ) => Promise<{ values: RegistryValue[]; next_after?: string }>;
}
export function useColumnFilters({
  registry,
  session,
  query,
  onChange,
  onSort,
}: {
  registry: string;
  session?: Session | null;
  query: object & { filters?: ColumnFilter[]; sort: string; descending: boolean };
  onChange: (filters: ColumnFilter[]) => void;
  onSort: (field: string, descending: boolean) => void;
}): ColumnFilterController {
  const [fields, setFields] = useState<RegistryField[]>([]);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    if (!session) return;
    const controller = new AbortController();
    setFailure('');
    request<{ fields: RegistryField[] }>(
      `/api/v1/${registry}/filter-values`,
      session ?? undefined,
      { registry: {} },
      controller.signal,
    )
      .then((result) => setFields(result.fields))
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [registry, session]);
  const load = useCallback(
    async (field: RegistryField, search: string, after?: string, signal?: AbortSignal) => {
      const body = { ...query, filters: filterRequest(query.filters) };
      const endpoint = `/api/v1/${registry}/filter-values`;
      const source = field.lookup;
      const sourceRequest = async (ids?: string[], cursor?: unknown) => {
        if (source === 'transportations') {
          const result = await request<{
            items: { id: string; container_number: string | null }[];
            next_cursor?: unknown;
          }>(
            '/api/v1/transportations/list',
            session ?? undefined,
            {
              limit: 40,
              sort: 'container_number',
              filters: ids
                ? [{ field: 'id', values: ids }]
                : search
                  ? [{ field: 'container_number', query: search }]
                  : undefined,
              ...(cursor ? { cursor } : {}),
            },
            signal,
          );
          return {
            items: result.items.map((item) => ({
              id: item.id,
              number: item.container_number || 'Без номера',
            })),
            next_cursor: result.next_cursor,
          };
        }
        if (source === 'shipments')
          return request<{ items: { id: string; number: string }[]; next_cursor?: unknown }>(
            '/api/v1/shipments/list',
            session ?? undefined,
            {
              limit: 40,
              ...(ids
                ? { filters: [{ field: 'id', values: ids }] }
                : { filters: search ? [{ field: 'number', query: search }] : undefined }),
              ...(cursor ? { cursor } : {}),
              sort: 'number',
            },
            signal,
          );
        const response = await request<{
          items: { id: string; name: string }[];
          next_cursor?: unknown;
        }>(
          '/api/v1/counterparties/list',
          session ?? undefined,
          {
            limit: 100,
            filters: ids
              ? [{ field: 'id', values: ids }]
              : search
                ? [{ field: 'search', query: search }]
                : undefined,
            ...(cursor ? { cursor } : {}),
            sort: 'name',
          },
          signal,
        );
        return {
          items: response.items.map((value) => ({ id: value.id, number: value.name })),
          next_cursor: response.next_cursor,
        };
      };
      if (source && search) {
        const found = await sourceRequest(undefined, after ? JSON.parse(after) : undefined);
        const labels = Object.fromEntries(found.items.map((item) => [item.id, item.number]));
        const available = await request<{ values: RegistryValue[] }>(
          endpoint,
          session ?? undefined,
          {
            registry: body,
            field: field.id,
            candidates: found.items.map((value) => value.id),
            limit: 100,
          },
          signal,
        );
        return {
          values: available.values.map((value) => ({
            ...value,
            label: labels[value.value] ?? value.label,
          })),
          next_after: found.next_cursor ? JSON.stringify(found.next_cursor) : undefined,
        };
      }
      const response = await request<{ values: RegistryValue[]; next_after?: string }>(
        endpoint,
        session ?? undefined,
        { registry: body, field: field.id, query: search || undefined, after, limit: 40 },
        signal,
      );
      if (source && response.values.length) {
        const found = await sourceRequest(response.values.map((value) => value.value));
        const labels = Object.fromEntries(found.items.map((item) => [item.id, item.number]));
        return {
          ...response,
          values: response.values.map((value) => ({
            ...value,
            label: labels[value.value] ?? 'Недоступная запись',
          })),
        };
      }
      return response;
    },
    [query, registry, session],
  );
  return {
    fields,
    filters: query.filters ?? [],
    sort: query.sort,
    descending: query.descending,
    failure,
    change: onChange,
    order: onSort,
    values: load,
  };
}
function valueLabel(field: RegistryField, value: string, label?: string) {
  if (!value) return 'Не заполнено';
  if (field.id === 'archived') return value === 'true' ? 'В архиве' : 'Действующий';
  if (field.kind === 'boolean') return value === 'true' ? 'Да' : 'Нет';
  if (field.id === 'type')
    return transportationTypes[value as keyof typeof transportationTypes] ?? label ?? value;
  if (field.id === 'mode') return modes[value as keyof typeof modes] ?? label ?? value;
  if (field.id === 'roles')
    return counterpartyRoles[value as keyof typeof counterpartyRoles] ?? label ?? value;
  return label || value;
}
export function filterLabel(field: RegistryField | undefined, filter: ColumnFilter) {
  if (filter.empty !== undefined) return filter.empty ? 'Не заполнено' : 'Заполнено';
  if (filter.query !== undefined) return `содержит «${filter.query}»`;
  if (filter.date_from || filter.date_to)
    return `${filter.date_from?.toDisplayString() ?? '…'} — ${filter.date_to?.toDisplayString() ?? '…'}`;
  return (
    filter.values
      ?.map((value) =>
        field
          ? valueLabel(field, value, filter.labels?.[value])
          : (filter.labels?.[value] ?? value),
      )
      .join(', ') ?? ''
  );
}
export function RegistryFilterToolbar({
  controller,
  onOpen,
  hidden,
  extraEntries = [],
  children,
}: {
  extraEntries?: { id: string; label: string; onOpen: (anchor: HTMLElement) => void }[];
  children?: ReactNode;
  controller: ColumnFilterController;
  onOpen: (fields: string[], anchor: HTMLElement) => void;
  hidden: Set<string>;
}) {
  const [picker, setPicker] = useState(false);
  return (
    <div className="registry-filter-toolbar">
      <div className="registry-filter-picker">
        <button type="button" aria-expanded={picker} onClick={() => setPicker(!picker)}>
          Фильтры{controller.filters.length ? ` · ${controller.filters.length}` : ''}
        </button>
        {picker && (
          <div className="registry-field-picker">
            {extraEntries.map((entry) => (
              <button
                key={entry.id}
                type="button"
                onClick={(event) => {
                  setPicker(false);
                  entry.onOpen(
                    event.currentTarget.parentElement!.parentElement!.querySelector('button')!,
                  );
                }}
              >
                {entry.label}
              </button>
            ))}
            {controller.fields.map((field) => (
              <button
                key={field.id}
                type="button"
                onClick={(event) => {
                  setPicker(false);
                  onOpen(
                    [field.id],
                    event.currentTarget.parentElement!.parentElement!.querySelector('button')!,
                  );
                }}
              >
                {field.label}
              </button>
            ))}
          </div>
        )}
      </div>
      {controller.filters.map((filter) => (
        <div className="registry-filter-chip" key={filter.field}>
          <button
            type="button"
            title={filterLabel(
              controller.fields.find((field) => field.id === filter.field),
              filter,
            )}
            onClick={(event) => onOpen([filter.field], event.currentTarget)}
          >
            {controller.fields.find((field) => field.id === filter.field)?.label ?? filter.field}:{' '}
            {filterLabel(
              controller.fields.find((field) => field.id === filter.field),
              filter,
            )}
            {hidden.has(filter.field) ? ' · скрытый столбец' : ''}
          </button>
          <button
            type="button"
            aria-label={`Сбросить фильтр «${controller.fields.find((field) => field.id === filter.field)?.label ?? filter.field}»`}
            onClick={() =>
              controller.change(controller.filters.filter((value) => value.field !== filter.field))
            }
          >
            ×
          </button>
        </div>
      ))}
      {children}
      {controller.fields.some((field) => field.id === controller.sort) && (
        <span className="registry-sort-summary">
          Порядок: {controller.fields.find((field) => field.id === controller.sort)?.label}{' '}
          {controller.descending ? '↓' : '↑'}
          {hidden.has(controller.sort) ? ' · скрытый столбец' : ''}
        </span>
      )}
      {controller.filters.length > 0 && (
        <button type="button" className="link" onClick={() => controller.change([])}>
          Сбросить фильтры
        </button>
      )}
      {controller.failure && (
        <p className="error" role="alert">
          {controller.failure}
        </p>
      )}
    </div>
  );
}
export function RegistryFilterEditor({
  controller,
  fields,
  anchor,
  onClose,
  header,
  onDraftChange,
  titleOverride,
}: {
  controller: ColumnFilterController;
  fields: string[];
  anchor: HTMLElement;
  onClose: () => void;
  header?: ReactNode;
  onDraftChange?: (filters: ColumnFilter[]) => void;
  titleOverride?: string;
}) {
  const ref = useRef<HTMLDivElement>(null);
  const title = useId();
  const [draft, setDraft] = useState(
    controller.filters.filter((filter) => fields.includes(filter.field)),
  );
  const [failure, setFailure] = useState('');
  const close = useEffectEvent(onClose);
  useEffect(() => {
    const node = ref.current!;
    node.showPopover();
    const rect = anchor.getBoundingClientRect();
    const popup = node.getBoundingClientRect();
    // Привязываемся к заголовку только при открытии. Изменение содержимого
    // и прокрутка страницы не должны переносить редактируемое окно.
    const initialLeft = rect.left;
    const initialTop = Math.max(
      12,
      Math.min(rect.bottom + 8, window.innerHeight - popup.height - 12),
    );
    function position() {
      const bounds = node.getBoundingClientRect();
      const top = Math.max(12, Math.min(initialTop, window.innerHeight - bounds.height - 12));
      node.style.left = `${Math.max(12, Math.min(initialLeft, window.innerWidth - bounds.width - 12))}px`;
      node.style.top = `${top}px`;
      node.style.maxHeight = `calc(100dvh - ${top + 12}px)`;
    }
    position();
    window.addEventListener('resize', position);
    function outside(event: PointerEvent) {
      if (!node.contains(event.target as Node) && !anchor.contains(event.target as Node)) close();
    }
    function scroll(event: Event) {
      // Прокрутка списка значений не закрывает окно; прокрутка таблицы — закрывает.
      if (event.target instanceof Node && node.contains(event.target)) return;
      close();
    }
    document.addEventListener('scroll', scroll, true);
    document.addEventListener('pointerdown', outside);
    return () => {
      document.removeEventListener('scroll', scroll, true);
      document.removeEventListener('pointerdown', outside);
      window.removeEventListener('resize', position);
    };
  }, [anchor]);
  const selected = controller.fields.filter((field) => fields.includes(field.id));
  return createPortal(
    <div
      ref={ref}
      popover="manual"
      role="dialog"
      aria-labelledby={title}
      className="registry-filter-popover"
      onKeyDown={(event) => {
        if (event.key === 'Escape') {
          event.stopPropagation();
          onClose();
          anchor.focus();
        }
      }}
    >
      <div className="registry-filter-title">
        <h2 id={title}>
          {titleOverride ?? (selected.length === 1 ? selected[0].label : 'Фильтры столбца')}
        </h2>
        <button type="button" aria-label="Закрыть фильтр" onClick={onClose}>
          ×
        </button>
      </div>
      <form
        onSubmit={(event) => {
          event.preventDefault();
          if (draft.some((filter) => (filter.values?.length ?? 0) > 200)) {
            setFailure('Можно выбрать до 200 значений одного поля');
            return;
          }
          if (
            draft.some(
              (filter) =>
                filter.date_from &&
                filter.date_to &&
                filter.date_from.toString() > filter.date_to.toString(),
            )
          ) {
            setFailure('Начало периода позже окончания');
            return;
          }
          controller.change(
            replaceFilters(
              controller.filters,
              fields,
              draft.filter(
                (value) =>
                  value.empty !== undefined ||
                  value.values?.length ||
                  value.query?.trim() ||
                  value.date_from ||
                  value.date_to,
              ),
            ),
          );
          onClose();
        }}
      >
        {header}
        <div className="registry-filter-scroll">
          {selected.map((field, index) => (
            <FilterSection
              primary={index === 0}
              grouped={selected.length > 1}
              key={field.id}
              controller={controller}
              field={field}
              value={draft.find((filter) => filter.field === field.id)}
              onChange={(value) => {
                const next = replaceFilters(draft, [field.id], value ? [value] : []);
                setDraft(next);
                onDraftChange?.(next);
              }}
            />
          ))}
        </div>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="registry-filter-actions">
          <button
            type="button"
            onClick={() => {
              setDraft([]);
              onDraftChange?.([]);
            }}
          >
            Очистить
          </button>
          <button className="primary" type="submit">
            Применить
          </button>
        </div>
      </form>
    </div>,
    anchor.closest('dialog') ?? document.body,
  );
}
function FilterSection({
  primary,
  grouped,
  controller,
  field,
  value,
  onChange,
}: {
  primary: boolean;
  grouped: boolean;
  controller: ColumnFilterController;
  field: RegistryField;
  value?: ColumnFilter;
  onChange: (value?: ColumnFilter) => void;
}) {
  const [expanded, setExpanded] = useState(primary);
  if (!grouped)
    return <FieldEditor controller={controller} field={field} value={value} onChange={onChange} />;
  return (
    <details
      className="registry-filter-section"
      open={expanded}
      onToggle={(event) => setExpanded(event.currentTarget.open)}
    >
      <summary>
        <svg
          viewBox="0 0 20 20"
          width="16"
          height="16"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.7"
          aria-hidden="true"
        >
          <path d="m7 4 6 6-6 6" />
        </svg>
        <span>{field.label}</span>
        {value && <small title={filterLabel(field, value)}>{filterLabel(field, value)}</small>}
      </summary>
      {expanded && (
        <FieldEditor controller={controller} field={field} value={value} onChange={onChange} />
      )}
    </details>
  );
}
function FieldEditor({
  controller,
  field,
  value,
  onChange,
}: {
  controller: ColumnFilterController;
  field: RegistryField;
  value?: ColumnFilter;
  onChange: (value?: ColumnFilter) => void;
}) {
  const [mode, setMode] = useState(
    value?.query !== undefined ? 'contains' : value?.empty !== undefined ? 'empty' : 'values',
  );
  const [search, setSearch] = useState('');
  const [page, setPage] = useState<RegistryValue[]>([]);
  const [next, setNext] = useState<string>();
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [paste, setPaste] = useState('');
  const [labels, setLabels] = useState<Record<string, string>>(value?.labels ?? {});
  const valuesScope = controller.scope?.(field);
  useEffect(() => {
    if (field.kind === 'date' || field.kind === 'boolean' || mode !== 'values') return;
    const controllerAbort = new AbortController();
    const timer = setTimeout(() => void load(undefined, controllerAbort.signal), search ? 200 : 0);
    return () => {
      clearTimeout(timer);
      controllerAbort.abort();
    };
  }, [search, field.id, mode, valuesScope]);
  async function load(after?: string, signal?: AbortSignal) {
    setBusy(true);
    setFailure('');
    try {
      const result = await controller.values(field, search, after, signal);
      if (signal?.aborted) return;
      setPage((old) => (after ? [...old, ...result.values] : result.values));
      setNext(result.next_after);
      setLabels((old) => ({
        ...old,
        ...Object.fromEntries(result.values.map((value) => [value.value, value.label])),
      }));
    } catch (error) {
      if (!signal?.aborted)
        setFailure(error instanceof Error ? error.message : 'Не удалось загрузить значения');
    } finally {
      if (!signal?.aborted) setBusy(false);
    }
  }
  const chosen = value?.values ?? [];
  function setValues(values: string[]) {
    values = [
      ...new Set(field.case_insensitive ? values.map((value) => value.toLowerCase()) : values),
    ];
    onChange(
      values.length
        ? {
            field: field.id,
            values,
            labels: Object.fromEntries(values.map((value) => [value, labels[value] ?? value])),
          }
        : undefined,
    );
  }
  function toggle(option: RegistryValue) {
    const values = chosen.includes(option.value)
      ? chosen.filter((value) => value !== option.value)
      : [...chosen, option.value];
    setLabels((old) => ({ ...old, [option.value]: option.label }));
    onChange(
      values.length
        ? { field: field.id, values, labels: { ...labels, [option.value]: option.label } }
        : undefined,
    );
  }
  const values =
    field.kind === 'boolean'
      ? [
          { value: 'true', label: '' },
          { value: 'false', label: '' },
        ]
      : page;
  return (
    <fieldset className="registry-field-editor" aria-label={field.label}>
      {field.sortable && (
        <div className="registry-filter-sort">
          <span>Сортировка</span>
          <button
            type="button"
            onClick={() => controller.order(field.id, false)}
            aria-pressed={controller.sort === field.id && !controller.descending}
          >
            По возрастанию ↑
          </button>
          <button
            type="button"
            onClick={() => controller.order(field.id, true)}
            aria-pressed={controller.sort === field.id && controller.descending}
          >
            По убыванию ↓
          </button>
        </div>
      )}
      {field.kind === 'values' && !field.lookup && (
        <div className="registry-filter-modes">
          <button
            type="button"
            aria-pressed={mode === 'values'}
            onClick={() => {
              setMode('values');
              onChange(undefined);
            }}
          >
            Выбрать значения
          </button>
          <button
            type="button"
            aria-pressed={mode === 'contains'}
            onClick={() => {
              setMode('contains');
              onChange({ field: field.id, query: '' });
            }}
          >
            По вхождению
          </button>
        </div>
      )}
      {value?.empty === true ? (
        <p className="muted">Только записи без значения</p>
      ) : field.kind === 'date' ? (
        <div className="registry-date-range">
          <label>
            С
            <DateInput
              value={value?.date_from ?? null}
              onChange={(date) =>
                onChange({
                  ...value,
                  empty: undefined,
                  field: field.id,
                  date_from: date ?? undefined,
                })
              }
            />
          </label>
          <label>
            По
            <DateInput
              value={value?.date_to ?? null}
              onChange={(date) =>
                onChange({
                  ...value,
                  empty: undefined,
                  field: field.id,
                  date_to: date ?? undefined,
                })
              }
            />
          </label>
        </div>
      ) : mode === 'contains' ? (
        <label>
          Содержит
          <input
            type="search"
            maxLength={256}
            value={value?.query ?? ''}
            onChange={(event) =>
              onChange(
                event.target.value ? { field: field.id, query: event.target.value } : undefined,
              )
            }
          />
        </label>
      ) : (
        <>
          {field.kind !== 'boolean' && (
            <label>
              Поиск значений
              <input
                type="search"
                placeholder="Начните вводить…"
                value={search}
                maxLength={256}
                onChange={(event) => setSearch(event.target.value)}
              />
            </label>
          )}
          {chosen.length > 0 && (
            <div className="registry-selected-values">
              {chosen.map((option) => (
                <button
                  key={option}
                  type="button"
                  onClick={() => setValues(chosen.filter((value) => value !== option))}
                >
                  {valueLabel(field, option, labels[option])} ×
                </button>
              ))}
            </div>
          )}
          <div className="registry-value-options" aria-busy={busy}>
            {values.map((option) => (
              <label key={option.value}>
                <input
                  type="checkbox"
                  checked={chosen.includes(option.value)}
                  onChange={() => toggle(option)}
                />
                <span>{valueLabel(field, option.value, option.label)}</span>
              </label>
            ))}
          </div>
          {busy && (
            <p className="hint" role="status">
              Загрузка…
            </p>
          )}
          {!busy && !values.length && !failure && (
            <p className="hint">Нет значений для выбранных условий</p>
          )}
          {failure && (
            <p className="error" role="alert">
              {failure}{' '}
              <button type="button" onClick={() => void load()}>
                Повторить
              </button>
            </p>
          )}
          {next && (
            <button type="button" disabled={busy} onClick={() => void load(next)}>
              Ещё значения
            </button>
          )}
          {field.kind === 'values' && !field.lookup && (
            <details>
              <summary>Вставить список</summary>
              <label>
                По одному значению в строке или из Excel
                <textarea
                  rows={3}
                  value={paste}
                  onChange={(event) => setPaste(event.target.value)}
                />
              </label>
              <button
                type="button"
                onClick={() => {
                  setValues([...new Set([...chosen, ...splitFilterValues(paste)])]);
                  setPaste('');
                }}
              >
                Добавить значения
              </button>
            </details>
          )}
        </>
      )}
      {field.kind !== 'boolean' &&
        (field.kind === 'date' || field.kind === 'set' || field.lookup || mode !== 'values') && (
          <label className="registry-empty-option">
            <input
              type="checkbox"
              checked={value?.empty === true}
              onChange={(event) => {
                setMode(event.target.checked ? 'empty' : 'values');
                onChange(event.target.checked ? { field: field.id, empty: true } : undefined);
              }}
            />
            Не заполнено
          </label>
        )}
      {value && (
        <button
          type="button"
          className="link"
          onClick={() => {
            onChange(undefined);
            setMode('values');
          }}
        >
          Убрать условие
        </button>
      )}
    </fieldset>
  );
}
