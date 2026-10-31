export const pretty = (value: unknown) => JSON.stringify(value, null, 2);
export const statusName = {
  draft: 'На подтверждении',
  published: 'Опубликован',
  cancelled: 'Отменён',
};
