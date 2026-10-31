export const statusName = {
  pending: 'Создаются',
  succeeded: 'Созданы',
  rejected: 'Отклонено',
} as const;
export const modes = {
  sea: 'Море',
  international_rail: 'Международное ЖД',
  international_road: 'Международное авто',
  rail: 'ЖД РФ',
  road: 'Вывоз',
  pickup: 'Вывоз',
  air: 'Авиа',
} as const;
export const transportationTypes = {
  consolidated: 'Сборка',
  container: 'Контейнер',
  air: 'Авиа',
  oversized: 'Негабарит',
  rail: 'ЖД',
  vessel: 'Судно',
} as const;
export const ownership = { COC: 'COC', SOC: 'SOC' } as const;
