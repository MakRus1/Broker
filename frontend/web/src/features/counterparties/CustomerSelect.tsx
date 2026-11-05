import { useRelatedCreate } from '../../shared/forms/RelatedCreation';
import { useLocalization } from '../localization/Localization';
import { LookupField } from '../../shared/ui/LookupField';
import { useCallback, useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
export type { Counterparty } from './counterparties';
import type { Counterparty } from './counterparties';

export function CustomerSelect({
  items,
  value,
  onChange,
  session,
  disabled,
  label,
  required = false,
  error = '',
  ownCompany = false,
  excludeId,
  allowCreate = true,
}: {
  items: Counterparty[];
  value: string;
  onChange: (id: string) => void;
  session: Session;
  disabled: boolean;
  label?: string;
  required?: boolean;
  error?: string;
  ownCompany?: boolean;
  excludeId?: string;
  allowCreate?: boolean;
}) {
  const { translate } = useLocalization();
  label ??= translate('field.customer');
  const [selected, setSelected] = useState<Counterparty>();
  useEffect(() => {
    setSelected((old) => (old?.id === value ? old : undefined));
    if (!value || items.some((item) => item.id === value)) return;
    const controller = new AbortController();
    void request<{ counterparty: Counterparty }>(
      '/api/v1/counterparties/get',
      session,
      { id: value },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setSelected(result.counterparty);
      })
      .catch(() => {
        /* Недоступную связанную компанию показываем без её данных. */
      });
    return () => controller.abort();
  }, [value, items, session]);
  const current = items.find((item) => item.id === value) ?? selected;
  const create = useRelatedCreate({
    kind: 'counterparty',
    defaults: { ownCompany },
    scope: JSON.stringify([session.subject, session.organization_id, ownCompany, excludeId]),
    enabled:
      allowCreate &&
      !disabled &&
      session.permissions.includes('counterparties.write') &&
      session.permissions.includes('counterparties.read'),
    onCreated(item) {
      setSelected(item);
      onChange(item.id);
    },
  });
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      if (!session.permissions.includes('counterparties.read')) return { items: [] };
      const result = await request<{ items: Counterparty[] }>(
        '/api/v1/counterparties/list',
        session,
        {
          filters: [
            { field: 'archived', values: ['false'] },
            ...(ownCompany ? [{ field: 'own_company', values: ['true'] }] : []),
            ...(query ? [{ field: 'search', query }] : []),
          ],
          limit: 100,
        },
        signal,
      );
      return {
        items: result.items.filter((item) => item.id !== excludeId),
        hasMore: result.items.length === 100,
      };
    },
    [session, ownCompany, excludeId],
  );
  return (
    <LookupField
      label={label}
      name={required ? 'customer_id' : undefined}
      value={value}
      selectedLabel={current?.name ?? (value ? translate('lookup.counterpartyUnavailable') : '')}
      placeholder={translate('lookup.counterpartyPlaceholder')}
      load={load}
      onCreate={create}
      createLabel={translate(
        ownCompany ? 'related.createOwnCompany' : 'related.createCounterparty',
      )}
      itemLabel={(item) => item.name}
      itemDescription={(item) => [item.country_code, item.tax_id].filter(Boolean).join(' · ')}
      disabled={disabled}
      required={required}
      error={error}
      validationMessage={translate('lookup.chooseCustomerDirectory')}
      onChange={(item) => {
        setSelected(item ?? undefined);
        onChange(item?.id ?? '');
      }}
    />
  );
}
