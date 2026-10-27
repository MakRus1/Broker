import { useEffect, useState } from 'react';
import { RegistryFilterPanel } from './RegistryFilterPanel';
import { Select } from './Select';
import { counterpartyRoles } from './counterparties';
import type { CounterpartyRegistryQuery } from './counterpartyRegistry';
export function CounterpartyFilters({
  value,
  loading,
  onApply,
}: {
  value: CounterpartyRegistryQuery;
  loading: boolean;
  onApply: (value: CounterpartyRegistryQuery) => void;
}) {
  const [draft, setDraft] = useState(value);
  useEffect(() => setDraft(value), [value]);
  const count = [value.query, value.role, value.country_code, value.status !== 'active'].filter(
    Boolean,
  ).length;
  return (
    <RegistryFilterPanel
      query={draft.query}
      onQueryChange={(query) => setDraft({ ...draft, query })}
      onApply={() => onApply({ ...draft, query: draft.query.trim() })}
      onReset={() =>
        onApply({ query: '', status: 'active', sort: value.sort, descending: value.descending })
      }
      count={count}
      loading={loading}
      label="Поиск"
      placeholder="Название или налоговый номер"
      columns={3}
    >
      <Select
        label="Роль"
        value={draft.role ?? ''}
        options={[
          { value: '', label: 'Все роли' },
          ...Object.entries(counterpartyRoles).map(([value, label]) => ({ value, label })),
        ]}
        onChange={(role) => setDraft({ ...draft, role: role || undefined })}
      />
      <Select
        label="Состояние"
        value={draft.status}
        options={[
          { value: 'active', label: 'Действующие' },
          { value: 'archived', label: 'Архив' },
          { value: 'all', label: 'Все' },
        ]}
        onChange={(status) =>
          setDraft({ ...draft, status: status as CounterpartyRegistryQuery['status'] })
        }
      />
      <label>
        Страна
        <input
          value={draft.country_code ?? ''}
          maxLength={2}
          pattern="[A-Z]{2}"
          placeholder="Код страны"
          onChange={(event) =>
            setDraft({ ...draft, country_code: event.target.value.toUpperCase() || undefined })
          }
        />
      </label>
    </RegistryFilterPanel>
  );
}
