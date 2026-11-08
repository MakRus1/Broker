import { SalesWorkFilters } from './SalesWorkFilters';
import { toggleWorkFilter } from './salesWorkFilters';
import { NextActionDate } from './NextActionDate';
import { useCallback, useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { RegistryHeading } from '../../shared/ui/RegistryHeading';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { RegistryTable, useRegistryColumns, type RegistryColumn } from '../registry/RegistryTable';
import { useColumnFilters } from '../registry/RegistryColumnFilters';
import {
  filterRequest,
  restoreColumnFilters,
  type ColumnFilter,
} from '../registry/registryFilters';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { useRegistryPage } from '../registry/useRegistryPage';
import { RegistryResultState } from '../../shared/ui/RegistryExperience';
import { useLocalization } from '../localization/Localization';
import type { Inquiry, SalesCard, SalesEntity, SalesResource, SalesSettings } from './types';
const baseRoute = (resource: SalesResource) => (resource === 'cards' ? '/sales' : '/inquiries');
export function SalesRegistry({
  resource,
  session,
  active,
  customer,
  settings,
  title = true,
}: {
  resource: SalesResource;
  session: Session;
  active: boolean;
  customer?: string;
  settings: SalesSettings;
  title?: boolean;
}) {
  const { translate } = useLocalization();
  const [query, setQuery] = useRegistryPreference<{
    sort: string;
    descending: boolean;
    filters?: ColumnFilter[];
  }>(
    preferenceKey(session.subject, `sales-${resource}`, 'query'),
    { sort: resource === 'cards' ? 'next_action_date' : 'number', descending: false },
    (raw) => {
      const value = raw && typeof raw === 'object' ? (raw as Record<string, unknown>) : {};
      return {
        sort: typeof value.sort === 'string' && value.sort.length <= 64 ? value.sort : 'id',
        descending: value.descending === true,
        filters: restoreColumnFilters(value.filters),
      };
    },
  );
  const filters = useColumnFilters({
    registry: `sales/${resource}`,
    session,
    query: { ...query, ...(customer ? { customer_id: customer } : {}) },
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const [version, setVersion] = useState(0);
  const [error, setError] = useState('');
  const [names, setNames] = useState<Record<string, string>>({});
  const loadPage = useCallback(
    async (
      cursor: { id: string; scope: string; text_value?: string; date_value?: string } | undefined,
      signal: AbortSignal,
    ) => {
      const result = await request<{
        items: SalesEntity[];
        next_cursor?: { id: string; scope: string; text_value?: string; date_value?: string };
      }>(
        `/api/v1/sales/${resource}/list`,
        session,
        {
          ...query,
          filters: filterRequest(query.filters),
          limit: 50,
          ...(cursor ? { cursor } : {}),
          ...(customer ? { customer_id: customer } : {}),
        },
        signal,
      );
      return { items: result.items, next: result.next_cursor };
    },
    [resource, session, customer, query],
  );
  const page = useRegistryPage<
    SalesEntity,
    { id: string; scope: string; text_value?: string; date_value?: string }
  >({
    active,
    refreshVersion: version,
    loadPage,
    onError: (e) => setError(e instanceof Error ? e.message : translate('sales.loadFailed')),
  });
  useEffect(() => {
    const ids = [...new Set(page.items.map((item) => item.customer_id))];
    if (!ids.length) return;
    const controller = new AbortController();
    request<{ items: { id: string; name: string }[] }>(
      '/api/v1/counterparties/list',
      session,
      { limit: 100, filters: [{ field: 'id', values: ids }] },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted)
          setNames(Object.fromEntries(items.map((i) => [i.id, i.name])));
      })
      .catch(() => {});
    return () => controller.abort();
  }, [page.items, session]);
  const definitions: RegistryColumn<SalesEntity>[] = [
    ...(resource === 'inquiries'
      ? [
          {
            id: 'number',
            sort: 'number',
            filterFields: ['number'],
            labelKey: 'sales.inquiry' as const,
            required: true,
            width: 180,
            render: (item: SalesEntity) => (item as Inquiry).number,
          },
        ]
      : []),
    {
      id: 'customer',
      filterFields: ['customer_id'],
      labelKey: 'sales.customer',
      required: resource === 'cards',
      width: 280,
      render: (item) => names[item.customer_id] ?? translate('common.loading'),
    },
    {
      id: 'manager',
      filterFields: ['manager_subject'],
      labelKey: 'sales.manager',
      width: 220,
      render: (item) => <EmployeeName subject={item.manager_subject} session={session} />,
    },
    ...(resource === 'cards'
      ? [
          {
            id: 'stage',
            sort: 'stage_id',
            filterFields: ['stage_id'],
            labelKey: 'sales.stage' as const,
            width: 200,
            render: (item: SalesEntity) =>
              settings.stages.find((stage) => stage.id === (item as SalesCard).stage_id)?.name ??
              '—',
          },
          {
            id: 'next_action',
            sort: 'next_action',
            filterFields: ['next_action', 'next_action_date'],
            labelKey: 'sales.nextAction' as const,
            width: 260,
            render: (item: SalesEntity) => {
              const card = item as SalesCard;
              return (
                <div className="sales-registry-action">
                  <span>{card.next_action || '—'}</span>
                  {card.next_action_date && <NextActionDate date={card.next_action_date} />}
                </div>
              );
            },
          },
        ]
      : [
          {
            id: 'status',
            sort: 'status',
            filterFields: ['status'],
            labelKey: 'sales.status' as const,
            width: 180,
            render: (item: SalesEntity) => translate(`sales.status.${(item as Inquiry).status}`),
          },
          {
            id: 'cargo',
            sort: 'cargo_description',
            filterFields: ['cargo_description'],
            labelKey: 'sales.cargo' as const,
            width: 300,
            render: (item: SalesEntity) => (item as Inquiry).cargo_description || '—',
          },
          {
            id: 'shipment',
            filterFields: false as const,
            labelKey: 'field.shipment' as const,
            width: 160,
            render: (item: SalesEntity) =>
              (item as Inquiry).shipment_id ? (
                <a
                  href={`#/shipments/${(item as Inquiry).shipment_id}`}
                  onClick={(e) => e.stopPropagation()}
                >
                  {translate('sales.openShipment')}
                </a>
              ) : (
                '—'
              ),
          },
        ]),
  ];
  const columns = useRegistryColumns(session.subject, `sales-${resource}`, definitions, {
    active,
    query,
    apply: (raw) => {
      const value = raw && typeof raw === 'object' ? (raw as Record<string, unknown>) : {};
      setQuery({
        sort: typeof value.sort === 'string' ? value.sort : 'id',
        descending: value.descending === true,
        filters: restoreColumnFilters(value.filters),
      });
    },
  });
  return (
    <>
      <RegistryHeading
        title={
          title ? translate(resource === 'cards' ? 'sales.funnel' : 'sales.inquiries') : undefined
        }
        count={page.items.length}
        columns={columns}
        loading={page.loading}
        onRefresh={() => {
          setError('');
          setVersion((v) => v + 1);
        }}
      >
        {session.permissions.includes('sales.write') && (
          <a
            className="button primary"
            href={`#${baseRoute(resource)}/new${customer ? `/${customer}` : ''}`}
          >
            {translate(resource === 'cards' ? 'sales.newCard' : 'sales.newInquiry')}
          </a>
        )}
      </RegistryHeading>
      {resource === 'cards' && (
        <SalesWorkFilters
          session={session}
          filters={query.filters}
          onToggle={(filter) =>
            setQuery((old) => ({ ...old, filters: toggleWorkFilter(old.filters, filter) }))
          }
        />
      )}
      <RegistryTable
        filters={filters}
        sort={query.sort}
        descending={query.descending}
        onSort={(sort) =>
          setQuery((old) => ({
            ...old,
            sort,
            descending: old.sort === sort ? !old.descending : false,
          }))
        }
        items={page.items}
        pageSource={page.source}
        columns={columns}
        definitions={definitions}
        onOpen={(item) => {
          location.hash = `${baseRoute(resource)}/${item.id}`;
        }}
        emptyMessage={
          <RegistryResultState
            loading={page.loading}
            failed={!!error}
            failureMessage={error}
            filtered={!!query.filters?.length}
            title={translate('sales.empty')}
            onRetry={() => setVersion((v) => v + 1)}
            onReset={() => setQuery((old) => ({ ...old, filters: [] }))}
          />
        }
      />
      <AutoLoadMore
        enabled={page.more}
        loading={page.loading}
        failed={!!error}
        onLoad={() => page.loadMore()}
      />
    </>
  );
}
