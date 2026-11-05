import { useLocalization } from '../localization/Localization';
import { useCallback } from 'react';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { LookupField } from '../../shared/ui/LookupField';
import { type TransportLocation, type TransportLocationKind } from '../routes/types';

export function TransportLocationLookup({
  label,
  kind,
  value,
  session,
  onChange,
}: {
  label: string;
  kind: TransportLocationKind;
  value?: TransportLocation;
  session: Session;
  onChange: (location?: TransportLocation) => void;
}) {
  const { translate } = useLocalization();
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      const result = await request<{ items: TransportLocation[] }>(
        '/api/geography/route-locations',
        session,
        { kind, query },
        signal,
      );
      return { items: result.items, hasMore: result.items.length === 20 };
    },
    [session, kind],
  );
  return (
    <LookupField
      label={label}
      value={value?.id ?? ''}
      selectedLabel={value ? [value.name, value.code].filter(Boolean).join(' · ') : ''}
      placeholder={translate('lookup.locationPlaceholder')}
      load={load}
      itemLabel={(item) => item.name}
      itemDescription={(item) => [item.code, item.country_code].filter(Boolean).join(' · ')}
      onChange={(item) => onChange(item ?? undefined)}
    />
  );
}
