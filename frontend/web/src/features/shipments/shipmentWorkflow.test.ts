import { expect, it } from 'vitest';
import { restoreWorkflow } from '../../shared/ui/GuidedWorkflow';
import {
  shipmentContextSteps,
  shipmentWorkflow,
  shipmentStepTab,
  shipmentStepEditing,
} from './shipmentWorkflow';

it('only offers finance when the user can view services', () => {
  expect(shipmentWorkflow(false).some((step) => step.id.startsWith('finance'))).toBe(false);
  expect(shipmentWorkflow(true).filter((step) => step.id.startsWith('finance'))).toHaveLength(3);
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

it('only includes plan steps when planned containers are enabled', () => {
  expect(shipmentWorkflow(true, false).some((step) => shipmentStepTab(step.id) === 'items')).toBe(
    false,
  );
  expect(
    shipmentWorkflow(true, true).filter((step) => shipmentStepTab(step.id) === 'items'),
  ).toHaveLength(3);
  expect(
    restoreWorkflow({ status: 'active', step: 'plan-create' }, shipmentWorkflow(true, false)).step,
  ).toBe('details');
});
it('opens route editing and maps detailed steps to existing tabs', () => {
  expect(shipmentStepEditing('route-edit')).toBe(true);
  expect(shipmentStepEditing('customer')).toBe(true);
  expect(shipmentStepEditing('route-details')).toBe(false);
  for (const step of shipmentWorkflow(true, true))
    expect(['details', 'route', 'items', 'transportations', 'finance']).toContain(
      shipmentStepTab(step.id),
    );
});

it('provides detailed guidance in every shipment tab without entering editing from view mode', () => {
  for (const editing of [false, true])
    for (const tab of ['details', 'route', 'items', 'transportations', 'finance']) {
      expect(shipmentContextSteps(tab, editing, true, true).length).toBeGreaterThan(1);
    }
  expect(shipmentContextSteps('details', false, true, false).map((step) => step.id)).toContain(
    'customer',
  );
  expect(shipmentContextSteps('items', false, true, false)).toEqual([]);
  expect(shipmentContextSteps('finance', false, false, false)).toEqual([]);
});

it('starts filling guidance at editable fields rather than a view-mode introduction', () => {
  const steps = shipmentContextSteps('details', true, true, true);
  expect(steps[0].id).toBe('customer');
  expect(steps.every((step) => shipmentStepEditing(step.id))).toBe(true);
  expect(steps[0].description).toBe('workflow.customerHint');
  expect(shipmentContextSteps('details', false, true, true)[1].description).toBe(
    'guide.shipmentCustomerView',
  );
});
