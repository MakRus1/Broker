import { afterEach, expect, it, vi } from 'vitest';
import { ApiError, request } from '../api/http';
import { saveErrorMessage } from './saveError';
import { interfaceText } from '../localization/interfaceText';

afterEach(() => vi.unstubAllGlobals());

it('preserves the server error code and trace for conflict handling', async () => {
  vi.stubGlobal(
    'fetch',
    vi
      .fn()
      .mockResolvedValue(
        new Response(JSON.stringify({ error: 'revision_conflict', message: 'Changed' }), {
          status: 409,
          headers: { 'X-Trace-Id': 'trace' },
        }),
      ),
  );
  await expect(request('/save')).rejects.toMatchObject({
    status: 409,
    code: 'revision_conflict',
    traceId: 'trace',
  });
});

it('explains revision conflicts without treating other conflicts as stale revisions', () => {
  expect(
    saveErrorMessage(new ApiError(409, 'Changed', undefined, 'revision_conflict'), 'Fallback'),
  ).toBe(interfaceText('validation.revisionConflict'));
  expect(
    saveErrorMessage(new ApiError(409, 'Invoice paid', undefined, 'invoice_paid'), 'Fallback'),
  ).toBe('Invoice paid');
  expect(saveErrorMessage(new Error('Offline'), 'Fallback')).toBe('Offline');
  expect(saveErrorMessage(null, 'Fallback')).toBe('Fallback');
});
