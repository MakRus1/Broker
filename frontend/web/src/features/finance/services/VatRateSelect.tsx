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
  const rate = parseVatRate(value);
  const [editingCustom, setCustom] = useState(false);
  const custom = editingCustom || rate === undefined || !rates.includes(rate);
  const current = rate === undefined ? '' : String(rate);
  return (
    <div className="vat-rate-select">
      <Select
        label={mode ? 'НДС' : 'Ставка НДС'}
        value={mode === 'exempt' ? 'exempt' : custom ? 'custom' : current}
        disabled={disabled}
        options={[
          ...(mode ? [{ value: 'exempt', label: 'Без НДС' }] : []),
          ...rates.map((rate) => ({ value: String(rate), label: `${formatVatRate(rate)}%` })),
          { value: 'custom', label: 'Другая ставка' },
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
          Ставка НДС, % · обязательно
          <DecimalInput
            name="vat_rate"
            required
            precision={2}
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
