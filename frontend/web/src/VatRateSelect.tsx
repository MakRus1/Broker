import { useState } from 'react';
import { Select } from './Select';
import { formatVatRate, parseVatRate } from './financialLabels';

// Быстрый выбор не ограничивает допустимые ставки: сохранённая нестандартная ставка остаётся доступной.
const rates = [0, 500, 700, 1000, 2000, 2200];
export function VatRateSelect({
  value,
  onChange,
  disabled = false,
}: {
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
}) {
  const rate = parseVatRate(value);
  const [custom, setCustom] = useState(rate === undefined || !rates.includes(rate));
  const current = rate === undefined ? '' : String(rate);
  return (
    <div className="vat-rate-select">
      <Select
        label="Ставка НДС"
        value={custom ? 'custom' : current}
        disabled={disabled}
        options={[
          ...rates.map((rate) => ({ value: String(rate), label: `${formatVatRate(rate)}%` })),
          { value: 'custom', label: 'Другая ставка' },
        ]}
        onChange={(next) => {
          setCustom(next === 'custom');
          if (next !== 'custom') onChange(formatVatRate(Number(next)));
        }}
      />
      {custom && (
        <label>
          Ставка НДС, % · обязательно
          <input
            name="vat_rate"
            required
            inputMode="decimal"
            disabled={disabled}
            value={value}
            onChange={(event) => onChange(event.target.value)}
            ref={(node) =>
              node?.setCustomValidity(
                parseVatRate(value) === undefined
                  ? 'Укажите ставку от 0 до 100% с точностью до сотых'
                  : '',
              )
            }
          />
        </label>
      )}
    </div>
  );
}
