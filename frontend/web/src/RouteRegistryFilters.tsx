import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import {
  RegistryFilterEditor,
  type ColumnFilterController,
  filterLabel,
} from './RegistryColumnFilters';
import {
  filterRequest,
  type ColumnFilter,
  type RegistryField,
  type RegistryValue,
} from './registryFilters';
import type { RegistryQuery, RouteFilterGroup } from './transportationRegistry';

export function useRouteRegistryFilters(
  session: Session | null | undefined,
  query: RegistryQuery,
  onChange: (groups: RouteFilterGroup[]) => void,
) {
  const [fields, setFields] = useState<RegistryField[]>([]);
  const [failure, setFailure] = useState('');
  const [anchor, setAnchor] = useState<HTMLElement>();
  useEffect(() => {
    if (!session?.permissions.includes('transportations.read')) return;
    const abort = new AbortController();
    request<{ fields: RegistryField[] }>(
      '/api/v1/transportations/filter-values',
      session,
      { registry: {}, scope: 'route' },
      abort.signal,
    )
      .then((value) => setFields(value.fields))
      .catch((error) => {
        if (!abort.signal.aborted) setFailure(error.message);
      });
    return () => abort.abort();
  }, [session]);
  const groups = query.route_filters ?? [];
  return {
    button: {
      active: groups.length > 0,
      expanded: Boolean(anchor),
      onOpen: (element: HTMLElement) => setAnchor(element),
    },
    toolbar: (
      <>
        {groups.map((group, index) => (
          <div className="registry-filter-chip" key={index}>
            <button
              type="button"
              onClick={(event) => setAnchor(event.currentTarget)}
              title={group.filters
                .map((value) =>
                  filterLabel(
                    fields.find((field) => field.id === value.field),
                    value,
                  ),
                )
                .join('; ')}
            >
              Участок {index + 1}:{' '}
              {group.filters
                .map(
                  (value) =>
                    `${fields.find((field) => field.id === value.field)?.label ?? value.field}: ${filterLabel(
                      fields.find((field) => field.id === value.field),
                      value,
                    )}`,
                )
                .join('; ')}
            </button>
            <button
              type="button"
              aria-label={`Убрать условия участка ${index + 1}`}
              onClick={() => onChange(groups.filter((_, i) => i !== index))}
            >
              ×
            </button>
          </div>
        ))}
        {groups.length > 0 && (
          <button type="button" className="link" onClick={() => onChange([])}>
            Сбросить маршрут
          </button>
        )}
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
      </>
    ),
    editor:
      anchor && session && fields.length > 0 ? (
        <RouteEditor
          session={session}
          query={query}
          fields={fields}
          anchor={anchor}
          onClose={() => setAnchor(undefined)}
          onChange={onChange}
        />
      ) : null,
  };
}
function RouteEditor({
  session,
  query,
  fields,
  anchor,
  onClose,
  onChange,
}: {
  session: Session;
  query: RegistryQuery;
  fields: RegistryField[];
  anchor: HTMLElement;
  onClose: () => void;
  onChange: (groups: RouteFilterGroup[]) => void;
}) {
  const [groups, setGroups] = useState<RouteFilterGroup[]>(
    query.route_filters?.length ? query.route_filters : [{ filters: [] }],
  );
  const [selected, setSelected] = useState(0);
  const current = groups[selected];
  const update = (filters: ColumnFilter[]) =>
    setGroups((old) => old.map((value, index) => (index === selected ? { filters } : value)));
  const controller: ColumnFilterController = {
    fields,
    filters: current.filters,
    sort: '',
    descending: false,
    failure: '',
    scope: (field) =>
      JSON.stringify(
        groups.map((group, index) => ({
          filters: filterRequest(
            index === selected
              ? group.filters.filter((filter) => filter.field !== field.id)
              : group.filters,
          ),
        })),
      ),
    order: () => {},
    change: (filters) => {
      const next = groups
        .map((value, index) => (index === selected ? { filters } : value))
        .filter((value) => value.filters.length);
      onChange(next);
    },
    values: (field, search, after, signal) =>
      request<{ values: RegistryValue[]; next_after?: string }>(
        '/api/v1/transportations/filter-values',
        session,
        {
          registry: {
            ...query,
            filters: filterRequest(query.filters),
            route_filters: [
              ...groups.filter((value, index) => index !== selected && value.filters.length),
              ...(current.filters.length ? [current] : []),
            ].map((value) => ({ filters: filterRequest(value.filters) })),
          },
          scope: 'route',
          field: field.id,
          group: groups.filter((value, index) => index !== selected && value.filters.length).length,
          query: search || undefined,
          after,
          limit: 40,
        },
        signal,
      ),
  };
  return (
    <RegistryFilterEditor
      key={selected}
      controller={controller}
      fields={fields.map((value) => value.id)}
      anchor={anchor}
      onClose={onClose}
      titleOverride="Фильтр маршрута"
      onDraftChange={update}
      header={
        <div className="route-filter-groups">
          <p>Все условия группы — на одном участке. Между группами — «и».</p>
          <div>
            {groups.map((group, index) => (
              <button
                type="button"
                key={index}
                aria-pressed={index === selected}
                onClick={() => setSelected(index)}
              >
                Участок {index + 1}
                {group.filters.length ? ` · ${group.filters.length}` : ''}
              </button>
            ))}
            {groups.length < 8 && (
              <button
                type="button"
                onClick={() => {
                  setGroups([...groups, { filters: [] }]);
                  setSelected(groups.length);
                }}
              >
                ＋ Участок
              </button>
            )}
            {groups.length > 1 && (
              <button
                type="button"
                onClick={() => {
                  setGroups(groups.filter((_, index) => index !== selected));
                  setSelected(Math.max(0, selected - 1));
                }}
              >
                Убрать группу
              </button>
            )}
          </div>
        </div>
      }
    />
  );
}
