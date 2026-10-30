import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import type { BankAccount } from './BankAccounts';
import { Select } from './Select';

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
  const [items, setItems] = useState<BankAccount[]>([]);
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  useEffect(() => {
    setItems([]);
    setFailure('');
    if (!owner) return;
    const controller = new AbortController();
    setLoading(true);
    request<{ items: BankAccount[] }>(
      '/api/v1/counterparties/bank-accounts/list',
      session,
      { counterparty_id: owner },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) setItems(result.items);
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [owner, session]);
  return (
    <>
      <Select
        label={label}
        value={value ?? ''}
        onChange={(id) => onChange(id || null)}
        disabled={!owner || loading}
        searchable
        options={[
          { value: '', label: loading ? 'Загружаем счета…' : 'Не выбран' },
          ...items
            .filter((item) => (item.currency === currency && !item.archived) || item.id === value)
            .map((item) => ({
              value: item.id,
              label: `${item.name} · ${item.number} · ${item.currency}${item.archived ? ' · в архиве' : ''}`,
            })),
          ...(value && !items.some((item) => item.id === value)
            ? [{ value, label: 'Выбранный счёт недоступен' }]
            : []),
        ]}
      />
      {failure && (
        <span className="error" role="alert">
          {failure}
        </span>
      )}
    </>
  );
}
