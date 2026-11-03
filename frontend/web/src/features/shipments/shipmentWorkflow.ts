import type { WorkflowStep } from '../../shared/ui/GuidedWorkflow';

export function shipmentWorkflow(finance: boolean): WorkflowStep[] {
  return [
    { id: 'details', title: 'workflow.detailsTitle', description: 'workflow.detailsHint' },
    { id: 'route', title: 'workflow.routeTitle', description: 'workflow.routeHint' },
    {
      id: 'transportations',
      title: 'workflow.transportTitle',
      description: 'workflow.transportHint',
    },
    ...(finance
      ? [
          {
            id: 'finance',
            title: 'workflow.financeTitle',
            description: 'workflow.financeHint',
          } as const,
        ]
      : []),
  ];
}
