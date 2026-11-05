import { renderToStaticMarkup } from 'react-dom/server';
import { expect, it } from 'vitest';
import { StationLookup } from './StationLookup';
import { TransportLocationLookup } from './TransportLocationLookup';
import { DirectoryLookup } from './DirectoryLookup';
import type { Session } from '../../shared/api/session';
const session: Session = {
  subject: 'test',
  actor: 'test',
  organization_id: 'org',
  display_name: 'Test',
  csrf_token: '',
  permissions: [],
};

it('keeps ESR, UIC, country and the unverified OSM marker in the station selection', () => {
  const html = renderToStaticMarkup(
    <StationLookup
      label="Station"
      session={session}
      value={{
        id: 'station',
        name: 'Test station',
        esr_code: '123456',
        uic_code: '1234567',
        esr_source: 'osm_hint',
        country_code: 'RU',
      }}
      onChange={() => {}}
    />,
  );
  expect(html).toContain('Test station · ЕСР 123456 (OSM) · UIC 1234567 · RU');
  expect(html).toContain('Очистить поле «Station»');
});
it('preserves international stations without ESR', () => {
  const html = renderToStaticMarkup(
    <StationLookup
      label="Station"
      session={session}
      value={{ id: 'station', name: 'Test station', uic_code: '8765432', country_code: 'DE' }}
      onChange={() => {}}
    />,
  );
  expect(html).toContain('Test station · UIC 8765432 · DE');
  expect(html).toContain('value="Test station · UIC 8765432 · DE"');
});
it('shows selected ports and airports with their codes in the shared control', () => {
  for (const kind of ['sea_port', 'airport'] as const) {
    const html = renderToStaticMarkup(
      <TransportLocationLookup
        label="Location"
        kind={kind}
        session={session}
        value={{ id: 'location', kind, name: 'Test location', code: 'ABC', country_code: 'RU' }}
        onChange={() => {}}
      />,
    );
    expect(html).toContain('value="Test location · ABC"');
    expect(html).toContain('class="lookup-field"');
  }
});
it('keeps a manually entered city visible without requiring a directory record', () => {
  const html = renderToStaticMarkup(
    <DirectoryLookup
      label="City"
      kind="cities"
      value="Новый город"
      session={session}
      onChange={() => {}}
    />,
  );
  expect(html).toContain('value="Новый город"');
  expect(html).toContain('Очистить поле «City»');
  expect(html).not.toContain('required=""');
});
