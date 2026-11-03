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
