import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it, vi } from 'vitest';
import { translate, type Locale } from '../../../../shared/i18n/catalog';
import { LookupField } from './LookupField';
import { CustomerSelect } from '../../features/counterparties/CustomerSelect';
import type { Counterparty } from '../../features/counterparties/counterparties';
import type { Session } from '../api/session';

let locale: Locale = 'ru';
vi.mock('../../features/localization/Localization', () => ({
  useLocalization: () => ({
    translate: (key: Parameters<typeof translate>[1], values?: Record<string, string | number>) =>
      translate(locale, key, values),
  }),
}));
const session: Session = {
  subject: 'test',
  actor: 'test',
  organization_id: 'org',
  display_name: 'Test',
  csrf_token: '',
  permissions: ['counterparties.read'],
};
it('shows the selected counterparty once and keeps clearing inside the control', () => {
  const html = renderToStaticMarkup(
    <CustomerSelect
      items={[{ id: 'company', name: 'Unique company' } as Counterparty]}
      value="company"
      session={session}
      disabled={false}
      onChange={() => {}}
      label="Клиент"
    />,
  );
  expect(html.match(/Unique company/g)).toHaveLength(1);
  expect(html).not.toContain('counterparty-selected');
  expect(html).toContain('Очистить поле «Клиент»');
  expect(html).toContain('value="Unique company"');
});
it('preserves required and disabled semantics without a clear action for empty values', () => {
  const html = renderToStaticMarkup(
    <LookupField
      label="Contract"
      value=""
      selectedLabel=""
      placeholder="Search"
      load={async () => ({ items: [] as { id: string }[] })}
      itemLabel={(item) => item.id}
      onChange={() => {}}
      required
      disabled
    />,
  );
  expect(html).toContain('required=""');
  expect(html).toContain('disabled=""');
  expect(html).toContain('aria-expanded="false"');
  expect(html).not.toContain('lookup-field-clear');
});
it('localizes the clear action and directory limit for both languages', () => {
  for (locale of ['ru', 'en'] as const) {
    const html = renderToStaticMarkup(
      <LookupField
        label="Contract"
        value="contract"
        selectedLabel="123"
        placeholder="Search"
        load={async () => ({ items: [] as { id: string }[] })}
        itemLabel={(item) => item.id}
        onChange={() => {}}
      />,
    );
    expect(html).toContain(
      translate(locale, 'lookup.clearField', { label: 'Contract' }).replaceAll('"', '&quot;'),
    );
    expect(translate(locale, 'lookup.moreResults')).not.toBe('lookup.moreResults');
  }
  locale = 'ru';
});

it('can keep a required current choice without a clearing button', () => {
  const html = renderToStaticMarkup(
    <LookupField
      label="View"
      value="current"
      selectedLabel="Role view"
      placeholder="View"
      load={async () => ({ items: [] as { id: string }[] })}
      itemLabel={(item) => item.id}
      onChange={() => {}}
      clearable={false}
    />,
  );
  expect(html).toContain('value="Role view"');
  expect(html).not.toContain('class="lookup-field-clear"');
  expect(html).toContain('data-clearable="false"');
});
