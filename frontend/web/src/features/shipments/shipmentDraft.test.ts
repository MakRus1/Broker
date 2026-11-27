import { expect, it } from 'vitest';
import { createPayload, draftOf, updatePayload } from './shipmentDraft';

it('sends the route and only a manually assigned number on creation', () => {
  const draft = { ...draftOf(), number: ' S-42 ', route_segments: [{ id: 'leg', type: 'sea' }] };
  expect(createPayload(draft, false)).toMatchObject({
    number: undefined,
    route_segments: draft.route_segments,
  });
  expect(createPayload(draft, true).number).toBe('S-42');
});

it('omits unchanged route and logist and sends explicit clears', () => {
  const shipment = {
    id: 'shipment',
    revision: 2,
    customer_id: 'customer',
    number: 'S-42',
    logist: 'logist',
    route_segments: [{ id: 'leg', type: 'road' }],
  };
  const baseline = draftOf(shipment);
  const copy = structuredClone(baseline);
  expect(updatePayload(copy, baseline, shipment.logist, true)).toMatchObject({
    route_segments: undefined,
    logist: undefined,
  });
  const cleared = { ...copy, route_segments: [], logist: '' };
  expect(updatePayload(cleared, baseline, shipment.logist, true)).toMatchObject({
    route_segments: [],
    logist: null,
  });
  expect(baseline).toMatchObject({ route_segments: shipment.route_segments, logist: 'logist' });
});
