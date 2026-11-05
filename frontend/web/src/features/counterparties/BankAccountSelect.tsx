import { useLocalization } from '../localization/Localization';
import { useCallback, useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import type { BankAccount } from './BankAccounts';
import { LookupField } from '../../shared/ui/LookupField';

export function BankAccountSelect({
  owner,
  label,
  value,
  currency,
  session,
  onChange,
}: {
  owner: string;
  label: string;
  value: string | null;
  currency: string;
  session: Session;
  onChange: (value: string | null) => void;
}) {
  const { translate } = useLocalization();
  const [items, setItems] = useState<BankAccount[]>([]);
  const fetchAccounts = useCallback(
    async (signal: AbortSignal) => {
      if (!owner) return [];
      const result = await request<{ items: BankAccount[] }>(
        '/api/v1/counterparties/bank-accounts/list',
        session,
        { counterparty_id: owner },
        signal,
      );
      if (!signal.aborted) setItems(result.items);
      return result.items;
    },
    [owner, session],
  );
  useEffect(() => {
    setItems([]);
    if (!value || !owner) return;
    const controller = new AbortController();
    void fetchAccounts(controller.signal).catch(() => {
      // The field retains its value; opening the directory offers a retry.
    });
    return () => controller.abort();
  }, [fetchAccounts, !!value]);
  const title = (item: BankAccount) =>
    `${item.name} · ${item.number} · ${item.currency}${item.archived ? translate('finance.archivedAccountSuffix') : ''}`;
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      const accounts = await fetchAccounts(signal);
      const search = query.toLocaleLowerCase();
      return {
        items: accounts.filter(
          (item) =>
            ((item.currency === currency && !item.archived) || item.id === value) &&
            `${item.name} ${item.number} ${item.bank_name}`.toLocaleLowerCase().includes(search),
        ),
      };
    },
    [fetchAccounts, currency, value],
  );
  const selected = items.find((item) => item.id === value);
  return (
    <LookupField
      label={label}
      value={value ?? ''}
      selectedLabel={
        selected ? title(selected) : value ? translate('finance.accountUnavailable') : ''
      }
      placeholder={translate('finance.lookupPlaceholder')}
      load={load}
      itemLabel={title}
      disabled={!owner}
      onChange={(item) => onChange(item?.id ?? null)}
    />
  );
}
