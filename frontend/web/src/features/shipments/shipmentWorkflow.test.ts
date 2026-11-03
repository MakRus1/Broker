import { expect, it } from 'vitest';
import { restoreWorkflow } from '../../shared/ui/GuidedWorkflow';
import { shipmentWorkflow } from './shipmentWorkflow';

it('only offers finance when the user can view services', () => {
  expect(shipmentWorkflow(false).map((step) => step.id)).toEqual([
    'details',
    'route',
    'transportations',
  ]);
  expect(shipmentWorkflow(true).at(-1)?.id).toBe('finance');
});
it('restores progress and validates obsolete or corrupt saved state', () => {
  const steps = shipmentWorkflow(true);
  expect(restoreWorkflow({ status: 'active', step: 'route' }, steps)).toEqual({
    status: 'active',
    step: 'route',
  });
  expect(restoreWorkflow({ status: 'completed', step: 'finance' }, steps).status).toBe('completed');
  expect(restoreWorkflow({ status: 'skipped', step: 'route' }, steps).status).toBe('skipped');
  expect(restoreWorkflow({ status: 'invalid', step: 'removed' }, steps)).toEqual({
    status: 'ready',
    step: 'details',
  });
  expect(restoreWorkflow(null, steps)).toEqual({ status: 'ready', step: 'details' });
  expect(restoreWorkflow({ status: 'active', step: 'finance' }, shipmentWorkflow(false)).step).toBe(
    'details',
  );
});
