import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { DateInput } from './DateInput';
import { RegistryFilterPanel } from './RegistryFilterPanel';
import { Select } from './Select';
import type { ShipmentRegistryQuery } from './shipmentRegistry';
export function ShipmentFilters({
  session,
  customers,
  value,
  loading,
  onApply,
}: {
  session: Session;
  customers: Counterparty[];
  value: ShipmentRegistryQuery;
  loading: boolean;
  onApply: (value: ShipmentRegistryQuery) => void;
}) {
  const [draft, setDraft] = useState(value);
  const [statuses, setStatuses] = useState<string[]>([]);
  const [failure, setFailure] = useState('');
  useEffect(() => setDraft(value), [value]);
  useEffect(() => {
    const controller = new AbortController();
    request<{ statuses: string[] }>('/api/v1/shipments/settings', session, {}, controller.signal)
      .then((settings) => setStatuses(settings.statuses))
      .catch((error: unknown) => {
        if (!controller.signal.aborted)
          setFailure(error instanceof Error ? error.message : 'Не удалось загрузить статусы');
      });
    return () => controller.abort();
  }, [session]);
  const count = [
    value.number_query,
    value.cargo_query,
    value.customer_id,
    value.statuses?.length,
    value.date_from || value.date_to,
  ].filter(Boolean).length;
  return (
    <RegistryFilterPanel
      searchFields={
        <>
          <label>
            Номер поставки
            <input
              type="search"
              value={draft.number_query ?? ''}
              maxLength={256}
              placeholder="Часть номера…"
              onChange={(event) => setDraft({ ...draft, number_query: event.target.value })}
            />
          </label>
          <label>
            Груз
            <input
              type="search"
              value={draft.cargo_query ?? ''}
              maxLength={256}
              placeholder="Часть описания…"
              onChange={(event) => setDraft({ ...draft, cargo_query: event.target.value })}
            />
          </label>
        </>
      }
      onApply={() =>
        onApply({
          ...draft,
          number_query: draft.number_query?.trim() || undefined,
          cargo_query: draft.cargo_query?.trim() || undefined,
        })
      }
      onReset={() => onApply({ sort: value.sort, descending: value.descending })}
      count={count}
      loading={loading}
    >
      <CustomerSelect
        label="Клиент"
        items={customers}
        value={draft.customer_id ?? ''}
        session={session}
        disabled={loading || !session.permissions.includes('counterparties.read')}
        onChange={(customer_id) => setDraft({ ...draft, customer_id: customer_id || undefined })}
      />
      <div>
        <Select
          label="Статус"
          value={draft.statuses?.[0] ?? ''}
          options={[
            { value: '', label: 'Все статусы' },
            ...statuses.map((status) => ({ value: status, label: status })),
          ]}
          onChange={(status) => setDraft({ ...draft, statuses: status ? [status] : undefined })}
        />
        {failure && (
          <p role="alert" className="field-error">
            {failure}
          </p>
        )}
      </div>
      <fieldset>
        <legend>Дата поставки</legend>
        <div className="registry-date-range">
          <label>
            С
            <DateInput
              aria-label="Дата поставки с"
              value={draft.date_from ?? null}
              max={draft.date_to}
              onChange={(date) => setDraft({ ...draft, date_from: date ?? undefined })}
            />
          </label>
          <label>
            По
            <DateInput
              aria-label="Дата поставки по"
              value={draft.date_to ?? null}
              min={draft.date_from}
              onChange={(date) => setDraft({ ...draft, date_to: date ?? undefined })}
            />
          </label>
        </div>
      </fieldset>
    </RegistryFilterPanel>
  );
}
