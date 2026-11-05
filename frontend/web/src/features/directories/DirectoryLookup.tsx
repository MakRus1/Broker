import { useLocalization } from '../localization/Localization';
import { useCallback, useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { LookupField } from '../../shared/ui/LookupField';

type Choice = { id: string; label: string; value: string; country?: string };
export function DirectoryLookup({
  session,
  kind,
  value,
  onChange,
  label,
}: {
  label: string;
  session: Session;
  kind: 'employees' | 'cities';
  value: string;
  onChange: (value: string, country?: string) => void;
}) {
  const { translate } = useLocalization();
  const [selected, setSelected] = useState<Choice>();
  const employees = kind === 'employees';
  useEffect(() => {
    if (!employees || !value) {
      setSelected(undefined);
      return;
    }
    const controller = new AbortController();
    request<{ items: { subject: string; display_name: string }[] }>(
      '/api/organizations/employees/list',
      session,
      { subject: value, limit: 1 },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted)
          setSelected({
            id: value,
            value,
            label: items[0]?.display_name ?? translate('ui.employeeUnavailable'),
          });
      })
      .catch(() => {
        if (!controller.signal.aborted)
          setSelected({ id: value, value, label: translate('ui.employeeLoadError') });
      });
    return () => controller.abort();
  }, [employees, value, session, translate]);
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      if (employees) {
        const result = await request<{ items: { subject: string; display_name: string }[] }>(
          '/api/organizations/employees/list',
          session,
          { query },
          signal,
        );
        return {
          items: result.items.map((item) => ({
            id: item.subject,
            value: item.subject,
            label: item.display_name,
          })),
        };
      }
      const result = await request<{
        items: { name: string; label: string; country_code: string }[];
      }>('/api/geography/cities', session, { query }, signal);
      return {
        items: result.items.map((item) => ({
          id: `${item.country_code}:${item.label}`,
          value: item.name,
          label: item.label,
          country: item.country_code,
        })),
      };
    },
    [employees, session],
  );
  return (
    <LookupField<Choice>
      label={label}
      value={value}
      selectedLabel={
        employees
          ? selected?.id === value
            ? selected.label
            : value
              ? translate('ui.employeeUnavailable')
              : ''
          : value
      }
      placeholder={translate(employees ? 'lookup.employeePlaceholder' : 'lookup.cityPlaceholder')}
      load={load}
      itemLabel={(item) => item.label}
      onChange={(item) => {
        setSelected(item ?? undefined);
        onChange(item?.value ?? '', item?.country);
      }}
      onTextChange={employees ? undefined : (text) => onChange(text)}
      minQueryLength={employees ? 0 : 2}
      footer={employees ? undefined : translate('lookup.dadata')}
    />
  );
}
