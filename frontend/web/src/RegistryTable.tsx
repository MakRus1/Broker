import {
  RegistryFilterToolbar,
  RegistryFilterEditor,
  type ColumnFilterController,
} from './RegistryColumnFilters';
import { useEffect, useRef, useState, useId, type ReactNode } from 'react';
import {
  preferenceKey,
  restoreColumns,
  moveColumn,
  placeColumn,
  useRegistryPreference,
  type ColumnDefinition,
} from './registryPreferences';

export function useRegistryColumns(
  subject: string,
  registry: string,
  definitions: readonly ColumnDefinition[],
) {
  const [columns, setColumns] = useRegistryPreference(
    preferenceKey(subject, registry, 'columns'),
    restoreColumns(null, definitions),
    (raw) => restoreColumns(raw, definitions),
  );
  return { columns, setColumns, definitions };
}
export type RegistryColumns = ReturnType<typeof useRegistryColumns>;
export function RegistryColumnSettings({ columns, setColumns, definitions }: RegistryColumns) {
  const ref = useRef<HTMLDivElement>(null);
  const trigger = useRef<HTMLButtonElement>(null);
  const [open, setOpen] = useState(false);
  const panelId = useId();
  useEffect(() => {
    function outside(event: PointerEvent) {
      if (!ref.current?.contains(event.target as Node)) setOpen(false);
    }
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
  return (
    <div
      ref={ref}
      className="registry-column-settings"
      onKeyDown={(event) => {
        if (event.key === 'Escape') {
          event.preventDefault();
          setOpen(false);
          trigger.current?.focus();
        }
      }}
    >
      <button
        type="button"
        ref={trigger}
        aria-expanded={open}
        aria-controls={panelId}
        onClick={() => setOpen(!open)}
      >
        Столбцы
      </button>
      {open && (
        <div id={panelId} className="registry-column-panel">
          <h2>Столбцы реестра</h2>
          <p>Перетащите заголовок, чтобы изменить порядок, а его границу — ширину.</p>
          {columns.map((column, index) => {
            const definition = definitions.find((entry) => entry.id === column.id)!;
            return (
              <div className="registry-column-option" key={column.id}>
                <label>
                  <input
                    type="checkbox"
                    checked={column.visible}
                    disabled={definition.required}
                    onChange={(event) =>
                      setColumns((old) =>
                        old.map((entry) =>
                          entry.id === column.id
                            ? { ...entry, visible: event.target.checked }
                            : entry,
                        ),
                      )
                    }
                  />
                  {definition.label}
                </label>
                <button
                  type="button"
                  aria-label={`Переместить «${definition.label}» влево`}
                  disabled={index === 0}
                  onClick={() => setColumns((old) => moveColumn(old, column.id, -1))}
                >
                  ←
                </button>
                <button
                  type="button"
                  aria-label={`Переместить «${definition.label}» вправо`}
                  disabled={index === columns.length - 1}
                  onClick={() => setColumns((old) => moveColumn(old, column.id, 1))}
                >
                  →
                </button>
              </div>
            );
          })}
          <button type="button" onClick={() => setColumns(restoreColumns(null, definitions))}>
            Сбросить столбцы
          </button>
        </div>
      )}
    </div>
  );
}
export interface RegistryColumn<T> extends ColumnDefinition {
  sort?: string;
  filterFields?: string[] | false;
  cellClassName?: string;
  render: (item: T) => ReactNode;
}
export function RegistryTable<T extends { id: string }>({
  items,
  columns: state,
  definitions,
  sort,
  descending,
  onSort,
  onOpen,
  filters,
  customFilters,
  filterSummary,
}: {
  filterSummary?: ReactNode;
  customFilters?: Record<
    string,
    { active: boolean; expanded?: boolean; onOpen: (anchor: HTMLElement) => void }
  >;
  filters?: ColumnFilterController;
  items: T[];
  columns: RegistryColumns;
  definitions: readonly RegistryColumn<T>[];
  sort?: string;
  descending?: boolean;
  onSort?: (field: string) => void;
  onOpen: (item: T) => void;
}) {
  const [filterEditor, setFilterEditor] = useState<{
    fields: string[];
    anchor: HTMLElement;
  } | null>(null);
  const drag = useRef<{ id: string; x: number; width: number } | null>(null);
  const scroll = useRef<HTMLDivElement>(null);
  const moving = useRef<string | null>(null);
  const pointerX = useRef<number | null>(null);
  const frame = useRef<number | null>(null);
  const ignoreClickUntil = useRef(0);
  const [source, setSource] = useState<string | null>(null);
  const [drop, setDrop] = useState<{ id: string; side: 'before' | 'after' } | null>(null);
  const visible = state.columns.filter((column) => column.visible);
  function targetAt(x: number) {
    const headers = Array.from(
      scroll.current?.querySelectorAll<HTMLElement>('th[data-column]') ?? [],
    );
    const target =
      headers.find((header) => x < header.getBoundingClientRect().right) ?? headers.at(-1);
    if (!target) return null;
    const rect = target.getBoundingClientRect();
    return {
      id: target.dataset.column!,
      side: x < rect.left + rect.width / 2 ? ('before' as const) : ('after' as const),
    };
  }
  function finishMove() {
    moving.current = null;
    pointerX.current = null;
    if (frame.current !== null) cancelAnimationFrame(frame.current);
    frame.current = null;
    setSource(null);
    setDrop(null);
    ignoreClickUntil.current = Date.now() + 200;
  }
  useEffect(
    () => () => {
      if (frame.current !== null) cancelAnimationFrame(frame.current);
    },
    [],
  );
  function autoScroll() {
    const node = scroll.current;
    if (!moving.current || !node) return;
    if (pointerX.current !== null) {
      const rect = node.getBoundingClientRect();
      const x = pointerX.current;
      const delta =
        x < rect.left + 60
          ? -Math.min(16, (rect.left + 60 - x) / 3)
          : x > rect.right - 60
            ? Math.min(16, (x - rect.right + 60) / 3)
            : 0;
      if (delta) {
        node.scrollLeft += delta;
        setDrop(targetAt(x));
      }
    }
    frame.current = requestAnimationFrame(autoScroll);
  }
  function resize(id: string, width: number) {
    state.setColumns((old) =>
      old.map((column) =>
        column.id === id
          ? { ...column, width: Math.max(120, Math.min(900, Math.round(width))) }
          : column,
      ),
    );
  }
  const hidden = new Set(
    (filters?.fields ?? [])
      .filter(
        (field) =>
          !visible.some((column) => {
            const definition = definitions.find((value) => value.id === column.id)!;
            return (
              definition.filterFields !== false &&
              (definition.filterFields ?? [definition.id]).includes(field.id)
            );
          }),
      )
      .map((field) => field.id),
  );
  return (
    <>
      {filters && (
        <RegistryFilterToolbar
          controller={filters}
          hidden={hidden}
          extraEntries={Object.entries(customFilters ?? {}).map(([id, value]) => ({
            id,
            label: definitions.find((column) => column.id === id)?.label ?? id,
            onOpen: value.onOpen,
          }))}
          children={filterSummary}
          onOpen={(fields, anchor) => setFilterEditor({ fields, anchor })}
        />
      )}
      {filters && filterEditor && (
        <RegistryFilterEditor
          controller={filters}
          fields={filterEditor.fields}
          anchor={filterEditor.anchor}
          onClose={() => {
            setFilterEditor(null);
            if (filterEditor.anchor.isConnected) filterEditor.anchor.focus();
          }}
        />
      )}
      <div
        className="table-scroll"
        ref={scroll}
        onDragOver={(event) => {
          if (!moving.current) return;
          event.preventDefault();
          event.dataTransfer.dropEffect = 'move';
          pointerX.current = event.clientX;
          setDrop(targetAt(event.clientX));
        }}
        onDragLeave={(event) => {
          if (!event.currentTarget.contains(event.relatedTarget as Node | null)) {
            pointerX.current = null;
            setDrop(null);
          }
        }}
        onDrop={(event) => {
          if (!moving.current) return;
          event.preventDefault();
          const target = targetAt(event.clientX);
          const id = moving.current;
          if (target) state.setColumns((old) => placeColumn(old, id, target.id, target.side));
          finishMove();
        }}
      >
        <table
          className="personal-registry-table"
          style={{ width: visible.reduce((sum, column) => sum + column.width, 0) }}
        >
          <colgroup>
            {visible.map((column) => (
              <col key={column.id} style={{ width: column.width }} />
            ))}
          </colgroup>
          <thead>
            <tr>
              {visible.map((column) => {
                const definition = definitions.find((entry) => entry.id === column.id)!;
                const filterFields =
                  definition.filterFields === false
                    ? []
                    : (definition.filterFields ?? [definition.id]).filter((id) =>
                        filters?.fields.some((field) => field.id === id),
                      );
                const customFilter = customFilters?.[column.id];
                const sortField =
                  definition.sort ??
                  filters?.fields.find((field) => field.id === column.id && field.sortable)?.id;
                return (
                  <th
                    key={column.id}
                    data-column={column.id}
                    className={source === column.id ? 'column-dragging' : undefined}
                    data-drop-side={
                      drop?.id === column.id && drop.id !== source ? drop.side : undefined
                    }
                    draggable
                    title="Перетащите заголовок для изменения порядка столбцов"
                    onDragStart={(event) => {
                      if (
                        drag.current ||
                        (event.target as HTMLElement).closest(
                          '.registry-column-resize,.registry-filter-button',
                        )
                      ) {
                        event.preventDefault();
                        return;
                      }
                      moving.current = column.id;
                      setSource(column.id);
                      event.dataTransfer.effectAllowed = 'move';
                      event.dataTransfer.setData('text/plain', column.id);
                      pointerX.current = event.clientX;
                      frame.current = requestAnimationFrame(autoScroll);
                    }}
                    onDragEnd={finishMove}
                    aria-sort={
                      sortField === sort ? (descending ? 'descending' : 'ascending') : undefined
                    }
                  >
                    <div className="registry-column-heading">
                      {sortField && onSort ? (
                        <button
                          type="button"
                          className="table-sort"
                          onClick={() => {
                            if (Date.now() >= ignoreClickUntil.current) onSort(sortField);
                          }}
                        >
                          <span className="registry-column-name">{definition.label}</span>
                          {sortField === sort && (
                            <span className="registry-column-order">{descending ? '↓' : '↑'}</span>
                          )}
                        </button>
                      ) : (
                        definition.label
                      )}
                      {(customFilter || (filters && filterFields.length > 0)) && (
                        <button
                          type="button"
                          className="registry-filter-button"
                          data-active={
                            customFilter?.active ||
                            filters?.filters.some((filter) =>
                              filterFields.includes(filter.field),
                            ) ||
                            undefined
                          }
                          aria-label={`Фильтр «${definition.label}»`}
                          aria-expanded={
                            customFilter
                              ? customFilter.expanded
                              : filterEditor?.anchor.dataset.columnFilter === column.id
                          }
                          data-column-filter={column.id}
                          onClick={(event) => {
                            event.stopPropagation();
                            if (customFilter) customFilter.onOpen(event.currentTarget);
                            else
                              setFilterEditor({
                                fields: filterFields,
                                anchor: event.currentTarget,
                              });
                          }}
                        >
                          <svg
                            width="16"
                            height="16"
                            viewBox="0 0 24 24"
                            fill="none"
                            stroke="currentColor"
                            strokeWidth="1.8"
                            aria-hidden="true"
                          >
                            <path d="M4 5h16M7 12h10M10 19h4" />
                          </svg>
                        </button>
                      )}
                    </div>
                    <span
                      role="separator"
                      aria-label={`Ширина столбца «${definition.label}»`}
                      aria-orientation="vertical"
                      aria-valuemin={120}
                      aria-valuemax={900}
                      aria-valuenow={column.width}
                      tabIndex={0}
                      className="registry-column-resize"
                      onKeyDown={(event) => {
                        if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
                          event.preventDefault();
                          resize(column.id, column.width + (event.key === 'ArrowLeft' ? -20 : 20));
                        }
                      }}
                      onPointerDown={(event) => {
                        event.preventDefault();
                        event.stopPropagation();
                        event.currentTarget.setPointerCapture(event.pointerId);
                        drag.current = { id: column.id, x: event.clientX, width: column.width };
                      }}
                      onPointerMove={(event) => {
                        if (drag.current?.id === column.id)
                          resize(column.id, drag.current.width + event.clientX - drag.current.x);
                      }}
                      onPointerUp={() => {
                        drag.current = null;
                      }}
                      onPointerCancel={() => {
                        drag.current = null;
                      }}
                    />
                  </th>
                );
              })}
            </tr>
          </thead>
          <tbody>
            {items.map((item) => (
              <tr key={item.id} className="clickable" onClick={() => onOpen(item)}>
                {visible.map((column) => (
                  <td
                    key={column.id}
                    className={definitions.find((entry) => entry.id === column.id)!.cellClassName}
                  >
                    {definitions.find((entry) => entry.id === column.id)!.render(item)}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
