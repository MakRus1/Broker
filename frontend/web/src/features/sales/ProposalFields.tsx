import { useEffect, useRef, useState } from 'react';
import { DecimalInput } from '../../shared/ui/DecimalInput';
import { Select } from '../../shared/ui/Select';
import { useLocalization } from '../localization/Localization';
import { currenciesWithCurrent } from '../finance/currencies';
import { VatRateSelect } from '../finance/services/VatRateSelect';
import { inputFixed, parseFixed } from '../finance/services/serviceCharges';
import { formatVatRate, parseVatRate } from '../finance/labels/financialLabels';
import type { ProposalPrice } from './proposals';
export function NumberField({
  label,
  value,
  digits = 2,
  max = 100000000,
  onChange,
}: {
  label: string;
  value: number;
  digits?: number;
  max?: number;
  onChange: (v: number) => void;
}) {
  const { translate } = useLocalization();
  const [text, setText] = useState(inputFixed(value, digits));
  const focused = useRef(false);
  useEffect(() => {
    if (!focused.current) setText(inputFixed(value, digits));
  }, [value, digits]);
  return (
    <label>
      {label}
      <DecimalInput
        required
        precision={digits}
        value={text}
        onFocus={() => {
          focused.current = true;
        }}
        onBlur={() => {
          focused.current = false;
        }}
        onChange={(e) => {
          const raw = e.target.value;
          setText(raw);
          const n = parseFixed(raw, digits, max);
          e.target.setCustomValidity(n === undefined ? translate('proposal.invalidNumber') : '');
          if (n !== undefined) onChange(n);
        }}
      />
    </label>
  );
}
export function PriceFields({
  value,
  onChange,
}: {
  value: ProposalPrice;
  onChange: (v: ProposalPrice) => void;
}) {
  const { translate, locale } = useLocalization();
  return (
    <div className="proposal-price-fields">
      <NumberField
        label={translate('proposal.price')}
        value={value.price_minor}
        onChange={(price_minor) => onChange({ ...value, price_minor })}
      />
      <Select
        label={translate('registryField.currency')}
        searchable
        value={value.currency}
        options={currenciesWithCurrent(value.currency, locale)}
        onChange={(currency) => onChange({ ...value, currency })}
      />
      <Select
        label={translate('finance.priceMode')}
        value={value.price_mode}
        options={['net', 'gross'].map((mode) => ({
          value: mode,
          label: translate(mode === 'net' ? 'proposal.priceNet' : 'proposal.priceGross'),
        }))}
        onChange={(price_mode) =>
          onChange({ ...value, price_mode: price_mode as ProposalPrice['price_mode'] })
        }
      />
      <VatRateSelect
        mode={value.vat_mode}
        value={formatVatRate(value.vat_rate_basis_points ?? 0)}
        onMode={(vat_mode) =>
          onChange({
            ...value,
            vat_mode,
            vat_rate_basis_points: vat_mode === 'exempt' ? undefined : 0,
          })
        }
        onChange={(v) =>
          onChange({ ...value, vat_mode: 'rate', vat_rate_basis_points: parseVatRate(v) })
        }
      />
    </div>
  );
}
