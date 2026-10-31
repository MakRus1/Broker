import { afterEach, describe, expect, it, vi } from 'vitest';
import { canApprove, parseValue, request, type Session } from './api';

const session: Session = {
  actor: 'issuer#author',
  subject: 'author',
  csrf_token: 'csrf',
  permissions: ['configs.approve'],
};

const draft = {
  status: 'draft' as const,
  author: 'issuer#author',
};

afterEach(() => {
  vi.unstubAllGlobals();
});

describe('редактор', () => {
  it.each(['null', '[]', '42', '"text"'])('отклоняет не объект %s', (value) => {
    expect(() => parseValue(value)).toThrow();
  });

  it('сохраняет JSON и не исполняет содержимое', () => {
    expect(parseValue('{"text":"<script>"}')).toEqual({ text: '<script>' });
  });

  it('не предлагает подтверждение своего драфта', () => {
    expect(canApprove(session, draft)).toBe(false);
  });

  it('разрешает самоподтверждение только с настройкой и правом', () => {
    expect(canApprove(session, draft, true)).toBe(true);
    expect(canApprove({ ...session, permissions: [] }, draft, true)).toBe(false);
    expect(canApprove(session, { ...draft, status: 'published' }, true)).toBe(false);
  });

  it('проверяет право и статус', () => {
    const other = { ...draft, author: 'issuer#other' };

    expect(canApprove(session, other)).toBe(true);
    expect(canApprove({ ...session, permissions: [] }, other)).toBe(false);
    expect(canApprove(session, { ...other, status: 'published' })).toBe(false);
  });
});

describe('BFF API', () => {
  it('передает cookie и CSRF, но не bearer', async () => {
    const fetch = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ ok: true }),
    });
    vi.stubGlobal('fetch', fetch);

    await request('/api/test', session, { value: 1 });

    expect(fetch).toHaveBeenCalledWith(
      '/api/test',
      expect.objectContaining({
        credentials: 'same-origin',
        headers: {
          'Content-Type': 'application/json',
          'X-CSRF-Token': 'csrf',
        },
      }),
    );
  });

  it.each([400, 401, 403, 409, 503])('сохраняет ошибку %s', async (status) => {
    const fetch = vi.fn().mockResolvedValue({
      ok: false,
      status,
      json: async () => ({ message: 'Конфликт версии' }),
    });
    vi.stubGlobal('fetch', fetch);

    await expect(request('/api/test')).rejects.toMatchObject({
      status,
      message: 'Конфликт версии',
    });
  });
});
