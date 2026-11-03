import type { MessageKey } from '../../../../../shared/i18n/catalog';
export type RecordGuideKind =
  | 'counterparty'
  | 'contract'
  | 'transportation'
  | 'service'
  | 'invoice'
  | 'receipt'
  | 'disbursement'
  | 'label';
export type RecordGuideStep = {
  id: string;
  label: MessageKey;
  description: MessageKey;
  selector?: string;
  viewDescription?: MessageKey;
};
const step = (id: string, label: MessageKey, description: MessageKey): RecordGuideStep => ({
  id,
  label,
  description,
});
export const recordGuides: Record<RecordGuideKind, RecordGuideStep[]> = {
  counterparty: [
    step('account-details', 'counterparty.bankAccountDetails', 'guide.bank'),
    step('bank-details', 'print.bankDetails', 'guide.bank'),
    step('account-options', 'record.additional', 'guide.bankArchive'),

    step('tax', 'counterparty.taxIDINN', 'guide.tax'),
    step('address', 'counterparty.registeredAddress', 'guide.address'),
    step('logist', 'shipment.logist', 'guide.logist'),

    step('legal-view', 'counterparty.legalName', 'guide.legal'),
    step('bank', 'counterparty.bankAccounts', 'guide.bank'),
    step('settlements', 'forms.outstandingBalances', 'guide.settlements'),
    step('legal', 'counterparty.legalEntity', 'guide.legal'),
    step('roles', 'registryField.roles', 'guide.roles'),
    step('contacts', 'counterparty.contacts', 'guide.contacts'),
  ],
  contract: [
    {
      ...step('identity', 'record.identification', 'guide.contractIdentity'),
      viewDescription: 'guide.contractIdentityView',
    },
    {
      ...step('parties', 'record.parties', 'guide.contractParties'),
      viewDescription: 'guide.contractPartiesView',
    },
    {
      ...step('payment', 'record.paymentTerms', 'guide.contractPayment'),
      viewDescription: 'guide.contractPaymentView',
    },
    {
      ...step('additional', 'record.additional', 'guide.additional'),
      viewDescription: 'guide.contractAdditionalView',
    },
  ],
  transportation: [
    step('type', 'registryField.type', 'guide.transportData'),
    step('ownership', 'forms.containerOwnership', 'guide.containerType'),
    step('container-number', 'registryField.container_number', 'guide.containerNumber'),
    step('container-type', 'registryField.container_type', 'guide.containerType'),
    step('hazard', 'registryField.hazardous', 'guide.hazard'),

    step('data', 'transport.details', 'guide.transportData'),
    step('conditions', 'transport.shipmentTerms', 'guide.transportTerms'),
    step('route', 'forms.transportationRoute', 'guide.transportRoute'),
  ],
  service: [
    step('requisites', 'record.identification', 'guide.serviceLabel'),
    step('parties', 'record.parties', 'guide.serviceParties'),
    step('calculation', 'record.calculation', 'guide.serviceCalculation'),
    step('currencies', 'record.currencyAndTax', 'guide.serviceCurrency'),
    step('amounts', 'record.serviceAmounts', 'guide.serviceAmounts'),
    step('additional', 'record.additional', 'guide.additional'),
  ],
  invoice: [
    step('date-view', 'registryField.invoice_date', 'guide.invoiceIdentity'),
    step('party-view', 'registryField.counterparty_id', 'guide.invoiceParties'),
    step('identity', 'record.identification', 'guide.invoiceIdentity'),
    step('parties', 'record.parties', 'guide.invoiceParties'),
    step('payment', 'record.paymentTerms', 'guide.invoicePayment'),
    step('services', 'nav.serviceCharges', 'guide.invoiceServices'),
    step('additional', 'record.additional', 'guide.additional'),
  ],
  receipt: [
    step('date-view', 'registryField.payment_date', 'guide.paymentIdentity'),
    step('party-view', 'forms.ownLegalEntityLabel', 'guide.receiptParties'),
    step('allocations', 'forms.allocatedInvoices', 'guide.paymentAllocation'),
    step('identity', 'record.identification', 'guide.paymentIdentity'),
    step('parties', 'record.parties', 'guide.receiptParties'),
    step('payment', 'record.paymentTerms', 'guide.paymentAllocation'),
    step('additional', 'record.additional', 'guide.additional'),
  ],
  disbursement: [
    step('date-view', 'registryField.payment_date', 'guide.paymentIdentity'),
    step('party-view', 'forms.ownLegalEntityLabel', 'guide.disbursementParties'),
    step('allocations', 'forms.allocatedInvoices', 'guide.paymentAllocation'),
    step('identity', 'record.identification', 'guide.paymentIdentity'),
    step('parties', 'record.parties', 'guide.disbursementParties'),
    step('payment', 'record.paymentTerms', 'guide.paymentAllocation'),
    step('additional', 'record.additional', 'guide.additional'),
  ],
  label: [
    step('direction', 'registryField.direction', 'guide.direction'),
    step('unit', 'registryField.unit', 'guide.unit'),
    step('currency', 'forms.defaultCurrency', 'guide.labelCurrency'),
    step('vat', 'forms.defaultVAT', 'guide.labelVAT'),

    step('calculation', 'record.calculation', 'guide.labelCalculation'),
    step('additional', 'record.additional', 'guide.additional'),
  ],
};

export const sharedRecordGuideSteps: RecordGuideStep[] = [
  { id: 'history-heading', label: 'forms.changeHistory', description: 'guide.historyEvents' },
  { id: 'history-empty-message', label: 'forms.noChangesYet', description: 'guide.historyEmpty' },

  {
    id: 'history-empty',
    label: 'finance.history',
    description: 'guide.historyEmpty',
    selector: '.counterparty-history > .hint, .financial-history > .empty-state',
  },
  {
    id: 'history-list',
    label: 'finance.history',
    description: 'guide.historyEvents',
    selector: '.financial-history, .counterparty-history',
  },
  {
    id: 'settlement-filters',
    label: 'forms.settlements',
    description: 'guide.settlements',
    selector: '.settlement-filters',
  },
  {
    id: 'settlement-totals',
    label: 'forms.outstandingBalances',
    description: 'guide.settlements',
    selector: '.settlement-table-wrap',
  },
  {
    id: 'settlement-detail',
    label: 'forms.settlementDocuments',
    description: 'guide.invoicePayment',
    selector: '.settlement-detail-heading',
  },

  {
    id: 'route-stages',
    label: 'registryField.route',
    description: 'guide.routeStages',
    selector: '.route-segment',
  },
  {
    id: 'route-collapse',
    label: 'route.expandAll',
    description: 'guide.routeCollapse',
    selector: '.route-collapse-actions, .route-view-actions',
  },
  {
    id: 'finance-summary',
    label: 'home.finance',
    description: 'guide.serviceAmounts',
    selector: '.finance-summary-disclosure',
  },
  {
    id: 'finance-services',
    label: 'nav.serviceCharges',
    description: 'guide.invoiceServices',
    selector: '.finance-services-table-wrap',
  },
  {
    id: 'bank-list',
    label: 'counterparty.bankAccounts',
    description: 'guide.bank',
    selector: '.bank-account-list',
  },
  {
    id: 'bank-archive',
    label: 'filter.archived',
    description: 'guide.bankArchive',
    selector: '.bank-archive-toggle',
  },
  {
    id: 'history-events',
    label: 'finance.history',
    description: 'guide.historyEvents',
    selector: '.history-entry, .financial-history-event',
  },
  {
    id: 'history-details',
    label: 'forms.stateAfterChange',
    description: 'guide.historyDetails',
    selector: '.history-detail, .counterparty-diff, .financial-history-diff',
  },
];

export const recordTabGuides: Partial<Record<MessageKey, MessageKey>> = {
  'counterparty.details': 'guide.tabRequisites',
  'shipment.details': 'guide.tabMain',
  'counterparty.bankAccounts': 'guide.tabAccounts',
  'forms.settlements': 'guide.tabSettlements',
  'finance.history': 'guide.tabHistory',
  'registryField.route': 'guide.tabRoute',
  'home.finance': 'guide.tabFinance',
};

export function recordGuideDescription(step: RecordGuideStep, editing: boolean): MessageKey {
  return editing ? step.description : (step.viewDescription ?? step.description);
}
