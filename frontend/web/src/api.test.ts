import { afterEach, describe, expect, it, vi } from 'vitest';
import { ApiError, Commands, request, type Session } from './api';
const session: Session = {
  actor: 'issuer#user',
  subject: 'user',
  display_name: 'Логист',
  csrf_token: 'csrf',
  permissions: [],
};
afterEach(() => vi.unstubAllGlobals());
describe('Браузерный API', () => {
  it('отправляет CSRF и cookie, не использует Bearer-токен в браузере', async () => {
    const fetch = vi.fn().mockResolvedValue(new Response(JSON.stringify({ items: [] })));
    vi.stubGlobal('fetch', fetch);
    await request('/api/v1/shipments/list', session, { limit: 50 });
    const init = fetch.mock.calls[0][1];
    expect(init.credentials).toBe('same-origin');
    expect(init.headers).toEqual({ 'Content-Type': 'application/json', 'X-CSRF-Token': 'csrf' });
  });
  it('показывает понятную ошибку и идентификатор трассы', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue(
        new Response(JSON.stringify({ message: 'Редакция изменилась' }), {
          status: 409,
          headers: { 'X-Trace-Id': 'trace' },
        }),
      ),
    );
    await expect(request('/api/test', session, {})).rejects.toMatchObject({
      status: 409,
      message: 'Редакция изменилась',
      traceId: 'trace',
    });
  });
  it('сохраняет ключ и исходную команду после сетевого сбоя, даже если редакция обновилась', async () => {
    const fetch = vi
      .fn()
      .mockRejectedValueOnce(new TypeError('Сеть недоступна'))
      .mockResolvedValueOnce(new Response('{}'));
    vi.stubGlobal('fetch', fetch);
    const commands = new Commands();
    await expect(
      commands.send('/api/create', session, { quantity: 3, expected_revision: 1 }),
    ).rejects.toThrow();
    await commands.send('/api/create', session, { quantity: 3, expected_revision: 2 });
    expect(fetch.mock.calls[1][1].body).toBe(fetch.mock.calls[0][1].body);
  });
  it('после явного конфликта позволяет отправить новую команду', async () => {
    const fetch = vi
      .fn()
      .mockResolvedValueOnce(new Response('{}', { status: 409 }))
      .mockResolvedValueOnce(new Response('{}'));
    vi.stubGlobal('fetch', fetch);
    const commands = new Commands();
    await expect(
      commands.send('/api/create', session, { expected_revision: 1 }),
    ).rejects.toBeInstanceOf(ApiError);
    await commands.send('/api/create', session, { expected_revision: 2 });
    const first = JSON.parse(fetch.mock.calls[0][1].body);
    const second = JSON.parse(fetch.mock.calls[1][1].body);
    expect(first.idempotency_key).not.toBe(second.idempotency_key);
    expect(second.expected_revision).toBe(2);
  });
});
