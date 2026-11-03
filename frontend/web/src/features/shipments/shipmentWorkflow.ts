import type { WorkflowStep } from '../../shared/ui/GuidedWorkflow';

export function shipmentWorkflow(finance: boolean, plannedContainers = false): WorkflowStep[] {
  return [
    { id: 'details', title: 'workflow.detailsTitle', description: 'workflow.detailsHint' },
    { id: 'customer', title: 'workflow.customerTitle', description: 'workflow.customerHint' },
    { id: 'recipient', title: 'workflow.recipientTitle', description: 'workflow.recipientHint' },
    { id: 'logist', title: 'workflow.logistTitle', description: 'workflow.logistHint' },
    { id: 'cargo', title: 'workflow.cargoTitle', description: 'workflow.cargoHint' },
    { id: 'terms', title: 'workflow.termsTitle', description: 'workflow.termsHint' },
    { id: 'delivery', title: 'workflow.deliveryTitle', description: 'workflow.deliveryHint' },
    { id: 'route', title: 'workflow.routeTitle', description: 'workflow.routeHint' },
    { id: 'route-edit', title: 'workflow.routeEditTitle', description: 'workflow.routeEditHint' },
    {
      id: 'route-details',
      title: 'workflow.routeDetailsTitle',
      description: 'workflow.routeDetailsHint',
    },
    ...(plannedContainers
      ? ([
          { id: 'items', title: 'workflow.planTitle', description: 'workflow.planHint' },
          {
            id: 'plan-quantities',
            title: 'workflow.planQuantitiesTitle',
            description: 'workflow.planQuantitiesHint',
          },
          {
            id: 'plan-create',
            title: 'workflow.planCreateTitle',
            description: 'workflow.planCreateHint',
          },
        ] as WorkflowStep[])
      : []),
    {
      id: 'transportations',
      title: 'workflow.transportTitle',
      description: 'workflow.transportHint',
    },
    {
      id: 'transport-list',
      title: 'workflow.transportListTitle',
      description: 'workflow.transportListHint',
    },
    ...(finance
      ? [
          {
            id: 'finance',
            title: 'workflow.financeTitle',
            description: 'workflow.financeHint',
          } as const,
          {
            id: 'finance-summary',
            title: 'workflow.financeSummaryTitle',
            description: 'workflow.financeSummaryHint',
          } as const,
          {
            id: 'finance-invoice',
            title: 'workflow.financeInvoiceTitle',
            description: 'workflow.financeInvoiceHint',
          } as const,
        ]
      : []),
  ];
}

export const shipmentFieldSteps = new Set([
  'customer',
  'recipient',
  'logist',
  'cargo',
  'terms',
  'delivery',
]);

export function shipmentStepTab(id: string): string {
  if (shipmentFieldSteps.has(id)) return 'details';
  if (id.startsWith('route-')) return 'route';
  if (id.startsWith('plan-')) return 'items';
  if (id.startsWith('transport-')) return 'transportations';
  if (id.startsWith('finance-')) return 'finance';
  return id;
}
export function shipmentStepEditing(id: string): boolean {
  return shipmentFieldSteps.has(id) || id === 'route-edit';
}

export function shipmentContextSteps(
  tab: string,
  editing: boolean,
  finance: boolean,
  planned: boolean,
): WorkflowStep[] {
  const steps = shipmentWorkflow(finance, planned).filter(
    (step) =>
      shipmentStepTab(step.id) === tab &&
      (tab === 'details' || (tab === 'route' && editing) || !shipmentStepEditing(step.id)),
  );
  if (editing) return steps.filter((step) => step.id !== 'details' && step.id !== 'route');
  const viewFields: Record<string, [WorkflowStep['title'], WorkflowStep['description']]> = {
    customer: ['field.customer', 'guide.shipmentCustomerView'],
    recipient: ['shipment.recipient', 'guide.shipmentRecipientView'],
    logist: ['shipment.logist', 'guide.logist'],
    cargo: ['shipment.cargoDescription', 'guide.shipmentCargoView'],
    terms: ['shipment.cargoTerms', 'guide.transportTerms'],
    delivery: ['shipment.delivery', 'guide.shipmentDeliveryView'],
  };
  return steps.map((step) =>
    viewFields[step.id]
      ? { ...step, title: viewFields[step.id][0], description: viewFields[step.id][1] }
      : step,
  );
}
