import { readFileSync } from 'node:fs';
import { runInNewContext } from 'node:vm';
import { expect, it, vi } from 'vitest';

const script = readFileSync(
  new URL(
    '../../../../../tools/keycloak/themes/broker/login/resources/js/localization.js',
    import.meta.url,
  ),
  'utf8',
);

it('loads public labels without credentials and restores embedded labels when removed', async () => {
  class Input {
    value = 'Sign in';
  }
  const button = new Input();
  const password = { textContent: 'Password' };
  const username = { textContent: 'Username' };
  const fields = {
    'meta[name="broker-localization-url"]': { content: 'https://web.example/localization/catalog' },
    'meta[name="broker-username-key"]': { content: 'username' },
    '#kc-login': button,
    'label[for="password"]': password,
    'label[for="username"]': username,
  };
  let messages = [
    { key: 'keycloak.doLogIn', value: 'Enter workspace' },
    { key: 'keycloak.password', value: '<b>Secret</b>' },
    { key: 'keycloak.username', value: '{invalid}' },
    { key: 'keycloak.unknown', value: 'Ignore' },
  ];
  const fetch = vi.fn(async () => ({ ok: true, json: async () => ({ messages }) }));
  const interval = vi.fn();
  runInNewContext(script, {
    document: {
      querySelector: (selector) => fields[selector],
      documentElement: { lang: 'en' },
      body: { dataset: { pageId: 'login-login' } },
      visibilityState: 'visible',
      addEventListener: vi.fn(),
    },
    window: { setInterval: interval },
    HTMLInputElement: Input,
    URL,
    AbortSignal,
    fetch,
  });
  await new Promise(setImmediate);
  expect(fetch.mock.calls[0][0].href).toBe('https://web.example/localization/catalog?locale=en');
  expect(fetch.mock.calls[0][1]).toMatchObject({
    credentials: 'omit',
    referrerPolicy: 'no-referrer',
  });
  expect(button.value).toBe('Enter workspace');
  expect(password.textContent).toBe('<b>Secret</b>');
  expect(username.textContent).toBe('Username');
  expect(Object.keys(password)).toEqual(['textContent']);
  messages = [];
  await interval.mock.calls[0][0]();
  await new Promise(setImmediate);
  expect(button.value).toBe('Sign in');
  expect(password.textContent).toBe('Password');
});
