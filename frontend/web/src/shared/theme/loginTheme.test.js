import { expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';
const script = readFileSync(
  new URL(
    '../../../../../tools/keycloak/themes/broker/login/resources/js/theme.js',
    import.meta.url,
  ),
  'utf8',
);
function login(search, saved, blocked = false) {
  const document = { documentElement: { dataset: {} } };
  const sessionStorage = {
    getItem: () => {
      if (blocked) throw Error('Storage unavailable');
      return saved;
    },
    setItem: (_key, value) => {
      if (blocked) throw Error('Storage unavailable');
      saved = value;
    },
  };
  runInNewContext(script, {
    window: { location: { search } },
    document,
    sessionStorage,
    URLSearchParams,
  });
  return { theme: document.documentElement.dataset.theme, saved };
}
it('передаёт выбор Web и сохраняет его на следующих шагах входа', () => {
  const first = login('?broker_theme=light', 'dark');
  expect(first).toEqual({ theme: 'light', saved: 'light' });
  expect(login('', first.saved).theme).toBe('light');
});
it('не использует неизвестные значения и работает с заблокированным хранилищем', () => {
  expect(login('?broker_theme=garbage', null).theme).toBe('system');
  expect(login('?broker_theme=dark', null, true).theme).toBe('dark');
  expect(login('', 'garbage').theme).toBe('system');
});
