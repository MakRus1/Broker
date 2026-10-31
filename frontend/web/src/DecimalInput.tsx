import type { ComponentPropsWithRef } from 'react';

export function normalizeDecimalInput(value: string, precision: number): string | undefined {
  const normalized = value.replace(/[\s\u00a0\u202f]/g, '');
  return new RegExp(`^\\d*(?:[.,]\\d{0,${precision}})?$`).test(normalized) ? normalized : undefined;
}

export function DecimalInput({
  precision = 2,
  onChange,
  ...props
}: ComponentPropsWithRef<'input'> & { precision?: number }) {
  return (
    <input
      {...props}
      inputMode="decimal"
      onChange={(event) => {
        const value = normalizeDecimalInput(event.target.value, precision);
        if (value === undefined) return;
        event.target.value = value;
        onChange?.(event);
      }}
    />
  );
}
