import { useCallback } from 'react';
import { useLocalization } from '../localization/Localization';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import type { RailwayStation } from '../routes/types';
import { LookupField } from '../../shared/ui/LookupField';

export function StationLookup({
  label,
  value,
  session,
  onChange,
}: {
  label: string;
  value?: RailwayStation;
  session: Session;
  onChange: (station?: RailwayStation) => void;
}) {
  const { translate } = useLocalization();
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      const result = await request<{ items: RailwayStation[] }>(
        '/api/geography/railway-stations',
        session,
        { query },
        signal,
      );
      return { items: result.items, hasMore: result.items.length === 20 };
    },
    [session],
  );
  const codes = (station: RailwayStation) =>
    [
      station.esr_code
        ? `${translate('forms.esrCode', { code: station.esr_code })}${station.esr_source === 'osm_hint' ? ' (OSM)' : ''}`
        : '',
      station.uic_code ? `UIC ${station.uic_code}` : '',
      station.country_code,
    ]
      .filter(Boolean)
      .join(' · ');
  return (
    <LookupField
      label={label}
      value={value?.id ?? ''}
      selectedLabel={value ? `${value.name} · ${codes(value)}` : ''}
      placeholder={translate('forms.stationNameESROrUIC')}
      load={load}
      itemLabel={(item) => item.name}
      itemDescription={codes}
      onChange={(item) => onChange(item ?? undefined)}
      footer={
        <>
          <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer">
            © OpenStreetMap
          </a>{' '}
          {translate('forms.esrFromOSMIsUnverified')}
        </>
      }
    />
  );
}
