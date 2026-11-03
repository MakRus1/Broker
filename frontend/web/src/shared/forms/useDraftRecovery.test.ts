import { expect, it } from 'vitest';
import { draftStorageKey, parseDraft } from './useDraftRecovery';
it('isolates drafts by actor, organization, form and record', () => {
  const session = { actor: 'issuer#user', organization_id: 'org-a' };
  const key = draftStorageKey(session, 'shipment', '1');
  expect(draftStorageKey({ ...session, organization_id: 'org-b' }, 'shipment', '1')).not.toBe(key);
  expect(draftStorageKey({ ...session, actor: 'another' }, 'shipment', '1')).not.toBe(key);
  expect(draftStorageKey(session, 'contract', '1')).not.toBe(key);
  expect(draftStorageKey(session, 'shipment', '2')).not.toBe(key);
});
it('keeps the source revision for safe restoration and expires old drafts', () => {
  const now = 10 * 86400000;
  const draft = { version: 1, revision: 7, savedAt: now - 1000, value: { number: 'S-1' } };
  expect(parseDraft(JSON.stringify(draft), now)).toEqual(draft);
  expect(parseDraft(JSON.stringify({ ...draft, savedAt: now - 7 * 86400000 }), now)).toBeNull();
  expect(parseDraft(JSON.stringify({ ...draft, savedAt: now + 1 }), now)).toBeNull();
});
it('ignores corrupt and unsupported draft payloads', () => {
  for (const raw of [
    null,
    'bad',
    'null',
    '{}',
    JSON.stringify({ version: 2, revision: 1, savedAt: 1, value: {} }),
    JSON.stringify({ version: 1, revision: -1, savedAt: 1, value: {} }),
    JSON.stringify({ version: 1, revision: 1, savedAt: 1, value: [] }),
  ])
    expect(parseDraft(raw, 2)).toBeNull();
});
