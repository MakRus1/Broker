import { useRelatedCreate } from '../../../shared/forms/RelatedCreation';
import { useLocalization } from '../../localization/Localization';
import { useCallback } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { LookupField } from '../../../shared/ui/LookupField';
import type { FinancialLabel } from '../labels/financialLabels';
import type { Contract } from '../../contracts/ContractsPage';

// Эти два справочника используют одинаковый поиск, но сохраняют тип выбранной модели.
export function ServiceChargeLookup<T extends FinancialLabel | Contract>({
  session,
  kind,
  label,
  value,
  name,
  counterparty,
  ownCompany,
  currency,
  allowCreate = true,
  disabled,
  required,
  onChange,
}: {
  session: Session;
  kind: 'labels' | 'contracts';
  label: string;
  value: string;
  name: string;
  counterparty?: string;
  ownCompany?: string;
  currency?: string;
  allowCreate?: boolean;
  disabled: boolean;
  required?: boolean;
  onChange: (value: T | null) => void;
}) {
  const { translate } = useLocalization();
  const create = useRelatedCreate({
    kind: 'contract',
    defaults: { counterparty_id: counterparty, own_company_id: ownCompany, currency },
    scope: JSON.stringify([
      session.subject,
      session.organization_id,
      counterparty,
      ownCompany,
      currency,
    ]),
    enabled:
      allowCreate &&
      kind === 'contracts' &&
      !disabled &&
      session.permissions.includes('contracts.write') &&
      session.permissions.includes('contracts.read'),
    onCreated(item) {
      onChange(item as T);
    },
  });
  const createLabel = useRelatedCreate({
    kind: 'financialLabel',
    defaults: {},
    scope: JSON.stringify([session.subject, session.organization_id, kind]),
    enabled:
      allowCreate &&
      kind === 'labels' &&
      !disabled &&
      session.permissions.includes('finance.read_financial_labels') &&
      session.permissions.includes('finance.write_financial_labels'),
    onCreated(item) {
      onChange(item as T);
    },
  });
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      const field = kind === 'labels' ? 'name' : 'number';
      const result = await request<{ items: T[] }>(
        kind === 'labels' ? '/api/v1/financial-labels/list' : '/api/v1/contracts/list',
        session,
        {
          sort: field,
          limit: 50,
          filters: [
            { field: 'archived', values: ['false'] },
            ...(query ? [{ field, query }] : []),
            ...(kind === 'contracts' && counterparty
              ? [{ field: 'counterparty_id', values: [counterparty] }]
              : []),
          ],
        },
        signal,
      );
      return { items: result.items, hasMore: result.items.length === 50 };
    },
    [kind, counterparty, session],
  );
  return (
    <LookupField
      label={label}
      value={value}
      selectedLabel={name}
      placeholder={translate('finance.lookupPlaceholder')}
      load={load}
      onCreate={kind === 'labels' ? createLabel : create}
      createLabel={translate(kind === 'labels' ? 'related.createLabel' : 'related.createContract')}
      itemLabel={(item) => ('name' in item ? item.name : item.number)}
      disabled={disabled}
      required={required}
      validationMessage={translate('finance.chooseLookup')}
      onChange={onChange}
    />
  );
}
