import { useLocalization } from '../../localization/Localization';
import { DecimalInput } from '../../../shared/ui/DecimalInput';
import { useState } from 'react';
import { Select } from '../../../shared/ui/Select';
import { formatVatRate, parseVatRate } from '../labels/financialLabels';

// Быстрый выбор не ограничивает допустимые ставки: сохранённая нестандартная ставка остаётся доступной.
const rates = [0, 500, 700, 1000, 2000, 2200];
export function VatRateSelect({
  value,
  onChange,
  disabled = false,
  mode,
  onMode,
}: {
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
  mode?: 'exempt' | 'rate';
  onMode?: (mode: 'exempt' | 'rate') => void;
}) {
  const { translate } = useLocalization();
  const rate = parseVatRate(value);
  const [editingCustom, setCustom] = useState(false);
  const custom = editingCustom || rate === undefined || !rates.includes(rate);
  const current = rate === undefined ? '' : String(rate);
  return (
    <div className="vat-rate-select">
      <Select
        label={
          mode ? translate('registryField.vat_mode') : translate('registryValue.vat_mode.rate')
        }
        value={mode === 'exempt' ? 'exempt' : custom ? 'custom' : current}
        disabled={disabled}
        options={[
          ...(mode ? [{ value: 'exempt', label: translate('registryValue.vat_mode.exempt') }] : []),
          ...rates.map((rate) => ({ value: String(rate), label: `${formatVatRate(rate)}%` })),
          { value: 'custom', label: translate('finance.customVatRate') },
        ]}
        onChange={(next) => {
          if (next === 'exempt') {
            onMode?.('exempt');
            return;
          }
          onMode?.('rate');
          setCustom(next === 'custom');
          if (next !== 'custom') onChange(formatVatRate(Number(next)));
        }}
      />
      {custom && mode !== 'exempt' && (
        <label>
          {translate('finance.vatRequired')}
          <DecimalInput
            name="vat_rate"
            required
            precision={2}
            disabled={disabled}
            value={value}
            onChange={(event) => onChange(event.target.value)}
            ref={(node) =>
              node?.setCustomValidity(
                parseVatRate(value) === undefined ? translate('finance.vatInvalid') : '',
              )
            }
          />
        </label>
      )}
    </div>
  );
}
