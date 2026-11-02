import { translate } from '../../../../shared/i18n/catalog';
import { readFileSync, readdirSync } from 'node:fs';
import { expect, it } from 'vitest';

it('keeps shared controls free of hardcoded Russian interface text', () => {
  const directory = new URL('../../shared/ui/', import.meta.url);
  for (const name of readdirSync(directory).filter(
    (name) => name.endsWith('.tsx') && !name.includes('.test.'),
  )) {
    const source = readFileSync(new URL(name, directory), 'utf8')
      .replace(/\/\*[\s\S]*?\*\//g, '')
      .replace(/^\s*\/\/.*$/gm, '');
    expect(source, name).not.toMatch(/[А-Яа-яЁё]/);
  }
});

it('uses the shared registry heading and keeps column settings out of section markup', () => {
  const pages = [
    '../../app/App.tsx',
    '../counterparties/CounterpartiesPage.tsx',
    '../contracts/ContractsPage.tsx',
    '../finance/labels/FinancialLabelsPage.tsx',
    '../finance/services/ServiceChargeRegistry.tsx',
    '../finance/invoices/InvoiceRegistry.tsx',
    '../finance/payments/PaymentRegistry.tsx',
  ];
  for (const path of pages) {
    const source = readFileSync(new URL(path, import.meta.url), 'utf8');
    expect(source, path).toContain('<RegistryHeading');
    expect(source, path).not.toContain('<RegistryColumnSettings');
  }
});

it('has an English heading for every standard registry column, including hidden ones', () => {
  const pages = [
    '../counterparties/CounterpartiesPage.tsx',
    '../contracts/ContractsPage.tsx',
    '../finance/labels/FinancialLabelsPage.tsx',
    '../finance/services/ServiceChargeRegistry.tsx',
    '../finance/invoices/InvoiceRegistry.tsx',
    '../finance/payments/PaymentRegistry.tsx',
    '../transportations/TransportationTable.tsx',
  ];
  let checked = 0;
  for (const path of pages) {
    const source = readFileSync(new URL(path, import.meta.url), 'utf8');
    for (const match of source.matchAll(/labelKey:\s*'([^']+)'/g)) {
      expect(translate('en', match[1]), `${path}: ${match[1]}`).not.toMatch(/[А-Яа-яЁё]/);
      checked++;
    }
  }

  expect(checked).toBeGreaterThan(30);
});
