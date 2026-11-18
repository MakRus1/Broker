import { Select } from '../../shared/ui/Select';
import type { Session } from '../../shared/api/session';
import { DirectoryLookup } from '../directories/DirectoryLookup';
import { useLocalization } from '../localization/Localization';

type DictionaryValue = { code: string; name: string };
export type ShipmentDictionaries = {
  incoterms: DictionaryValue[];
  carriage_terms: DictionaryValue[];
  loading: DictionaryValue[];
  route_types: DictionaryValue[];
};
export type ShipmentConditions = {
  incoterms: string;
  carriage_terms: string;
  loading: string;
  delivery_city: string;
  delivery_country: string;
  delivery_address: string;
};

export function ShipmentConditionsFields({
  value,
  dictionaries,
  session,
  onChange,
  section,
}: {
  value: Partial<ShipmentConditions>;
  dictionaries?: ShipmentDictionaries;
  session: Session;
  onChange: (patch: Partial<ShipmentConditions>) => void;
  section: 'terms' | 'delivery';
}) {
  const { translate } = useLocalization();
  function options(items: DictionaryValue[], current?: string) {
    return [
      { value: '', label: translate('ui.notSelected') },
      ...items.map((item) => ({ value: item.code, label: item.name })),
      ...(current && !items.some((item) => item.code === current)
        ? [{ value: current, label: current }]
        : []),
    ];
  }
  if (section === 'terms')
    return (
      <div className="shipment-fields three" data-workflow-field="terms">
        {(['incoterms', 'carriage_terms', 'loading'] as const).map((field) => (
          <Select
            key={field}
            label={
              field === 'incoterms'
                ? 'Incoterms'
                : translate(
                    field === 'carriage_terms' ? 'shipment.carriageTerms' : 'shipment.loading',
                  )
            }
            value={value[field] ?? ''}
            options={options(dictionaries?.[field] ?? [], value[field])}
            disabled={!dictionaries}
            onChange={(next) => onChange({ [field]: next })}
          />
        ))}
      </div>
    );
  return (
    <div className="shipment-fields" data-workflow-field="delivery">
      <DirectoryLookup
        session={session}
        kind="cities"
        label={translate('shipment.deliveryCity')}
        value={value.delivery_city ?? ''}
        onChange={(city, country) =>
          onChange({ delivery_city: city, ...(country ? { delivery_country: country } : {}) })
        }
      />
      <label>
        {translate('shipment.countryCode')}
        <input
          maxLength={2}
          pattern="[A-Z]{2}"
          placeholder="RU"
          value={value.delivery_country ?? ''}
          onChange={(event) => onChange({ delivery_country: event.target.value.toUpperCase() })}
        />
      </label>
      <label className="full">
        {translate('shipment.deliveryAddress')}
        <textarea
          maxLength={1000}
          rows={2}
          value={value.delivery_address ?? ''}
          onChange={(event) => onChange({ delivery_address: event.target.value })}
        />
      </label>
    </div>
  );
}
