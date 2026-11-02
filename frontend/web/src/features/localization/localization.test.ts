import { registryFieldLabel, registryValueLabel } from '../registry/registryLabels';
import { filterLabel } from '../registry/RegistryColumnFilters';
import { describe, expect, it } from 'vitest';
import { resolveLocale, restoreLanguage, translate } from '../../../../shared/i18n/catalog';
import { ru } from '../../../../shared/i18n/ru';
import { en } from '../../../../shared/i18n/en';
import { workspaceTitle, localizedWorkspaceTitle } from '../../app/workspace';

describe('локализация', () => {
  it('берёт первый поддерживаемый язык браузера и учитывает явный выбор', () => {
    expect(resolveLocale('auto', ['de-DE', 'en-US', 'ru-RU'])).toBe('en');
    expect(resolveLocale('auto', ['ru_RU'])).toBe('ru');
    expect(resolveLocale('auto', ['de-DE'])).toBe('ru');
    expect(resolveLocale('ru', ['en-US'])).toBe('ru');
    expect(restoreLanguage('invalid')).toBe('auto');
  });
  it('содержит одинаковые ключи и параметры в обоих каталогах', () => {
    expect(Object.keys(en).sort()).toEqual(Object.keys(ru).sort());
    for (const key of Object.keys(ru) as (keyof typeof ru)[]) {
      expect(en[key].match(/\{\w+\}/g)?.sort()).toEqual(ru[key].match(/\{\w+\}/g)?.sort());
      expect(en[key].trim()).not.toBe('');
    }
  });
  it('подставляет параметры как обычный текст', () => {
    expect(translate('en', 'common.loaded', { count: 42 })).toBe('Loaded: 42');
    expect(translate('ru', 'tabs.close', { title: '<script>' })).toBe('Закрыть вкладку <script>');
  });
  it('переводит идентификаторы полей и системные значения, сохраняя неизвестные подписи', () => {
    expect(registryFieldLabel('shipment_date', 'Дата поставки', 'en')).toBe('Shipment date');
    expect(registryFieldLabel('custom_field', 'Моё поле', 'en')).toBe('Моё поле');
    expect(registryFieldLabel('number', 'Номер договора', 'ru')).toBe('Номер договора');
    expect(registryValueLabel('type', 'container', 'en')).toBe('Container');
    expect(registryValueLabel('name', 'container', 'en')).toBeUndefined();
  });
  it('переводит условия фильтра без изменения пользовательских значений', () => {
    expect(filterLabel(undefined, { field: 'name', query: 'ООО Тест' }, 'en')).toBe(
      'contains “ООО Тест”',
    );
    expect(filterLabel(undefined, { field: 'name', empty: true }, 'en')).toBe('Not filled in');
    expect(
      filterLabel(
        { id: 'archived', label: 'Состояние', kind: 'boolean', sortable: true },
        { field: 'archived', values: ['false'] },
        'en',
      ),
    ).toBe('Active');
    expect(filterLabel(undefined, { field: 'name', values: ['S-42'] }, 'en')).toBe('S-42');
  });
  it('переводит служебные вкладки и сохраняет номера объектов', () => {
    expect(workspaceTitle('/shipments', 'en')).toBe('Shipments');
    expect(localizedWorkspaceTitle({ id: 's', route: '/shipments', title: 'Поставки' }, 'en')).toBe(
      'Shipments',
    );
    expect(
      localizedWorkspaceTitle(
        { id: 's', route: '/shipments/11111111-1111-4111-8111-111111111111', title: 'LCB-42' },
        'en',
      ),
    ).toBe('LCB-42');
  });
});
