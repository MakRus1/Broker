import type { HTMLAttributes } from 'react';
import type { ServiceCharge } from './serviceCharges';

// Одинаковые навигация и клавиатура во всех списках услуг.
export function serviceChargeRow(
  charge: ServiceCharge,
  onOpen: ((charge: ServiceCharge) => void) | undefined,
  readOnly = false,
): HTMLAttributes<HTMLTableRowElement> {
  if (!onOpen) return {};
  return {
    className: 'service-row-openable',
    tabIndex: 0,
    'aria-label': `${readOnly ? 'Просмотреть' : 'Редактировать'} услугу «${charge.label_name}»`,
    onClick: (event) => {
      if (
        (event.target as HTMLElement).closest(
          'a,button,input,select,textarea,summary,details,dialog,[role="button"],[role="combobox"]',
        ) ||
        window.getSelection()?.toString()
      )
        return;
      onOpen(charge);
    },
    onKeyDown: (event) => {
      if (event.target === event.currentTarget && event.key === 'Enter') {
        event.preventDefault();
        onOpen(charge);
      }
    },
  };
}
