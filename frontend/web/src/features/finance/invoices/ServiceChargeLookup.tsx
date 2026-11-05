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
  disabled: boolean;
  required?: boolean;
  onChange: (value: T | null) => void;
}) {
  const { translate } = useLocalization();
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
      itemLabel={(item) => ('name' in item ? item.name : item.number)}
      disabled={disabled}
      required={required}
      validationMessage={translate('finance.chooseLookup')}
      onChange={onChange}
    />
  );
}
