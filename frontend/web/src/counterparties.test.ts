import { describe, expect, it } from 'vitest';
import { counterpartyDetails, restoreCounterpartyDraft } from './counterparties';
const fallback = {
  id: '11111111-1111-4111-8111-111111111111',
  baseRevision: 2,
  value: counterpartyDetails(),
};
describe('Черновик контрагента', () => {
  it('сохраняет UUID повтора создания и незаполненные контакты при переходе между страницами', () => {
    const draft = {
      ...fallback,
      id: '22222222-2222-4222-8222-222222222222',
      baseRevision: 0,
      value: {
        ...counterpartyDetails(),
        name: 'Новая компания',
        contacts: [{ name: '', position: '', email: '', phone: '' }],
      },
    };
    expect(restoreCounterpartyDraft(JSON.stringify(draft), fallback)).toEqual(draft);
  });
  it('не подменяет редакцию сохранённого черновика актуальной серверной редакцией', () => {
    const old = {
      ...fallback,
      baseRevision: 1,
      value: { ...counterpartyDetails(), name: 'Старое изменение' },
    };
    expect(restoreCounterpartyDraft(JSON.stringify(old), fallback).baseRevision).toBe(1);
  });
  it.each([
    '{',
    JSON.stringify({ id: 'wrong', value: {} }),
    JSON.stringify({ ...fallback, value: { ...counterpartyDetails(), roles: ['toString'] } }),
    JSON.stringify({ ...fallback, value: { ...counterpartyDetails(), contacts: [{}] } }),
  ])('не восстанавливает повреждённое состояние %s', (raw) => {
    expect(restoreCounterpartyDraft(raw, fallback)).toEqual(fallback);
  });
});
