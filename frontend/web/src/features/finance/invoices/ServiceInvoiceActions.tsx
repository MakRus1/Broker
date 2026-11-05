import { useLocalization } from '../../localization/Localization';
import { useCallback, useEffect, useRef, useState, type ReactNode } from 'react';
import { useRelatedCreate } from '../../../shared/forms/RelatedCreation';
import { LookupField } from '../../../shared/ui/LookupField';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { Dialog } from '../../../shared/ui/Dialog';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed, type ServiceCharge } from '../services/serviceCharges';
import type { Invoice, InvoiceDetails, InvoiceRegistryItem, InvoiceDirection } from './invoices';

type ServiceContext = Pick<
  ServiceCharge,
  | 'direction'
  | 'counterparty_id'
  | 'counterparty_name'
  | 'contract_id'
  | 'contract_number'
  | 'invoice_currency'
>;
type Creation = {
  id: string;
  direction: InvoiceDirection;
  number: string | null;
  invoice_date: string;
  due_date: string | null;
  own_company_id: string;
  counterparty_id: string;
  own_bank_account_id: string | null;
  counterparty_bank_account_id: string | null;
  contract_id: string | null;
  currency: string;
  note: string | null;
  archived: boolean;
};
export type ServiceInvoiceSelection =
  { kind: 'existing'; invoice: Invoice } | { kind: 'new'; body: Creation };
export async function attachServiceInvoice(
  serviceChargeId: string,
  selection: ServiceInvoiceSelection,
  session: Session,
  previousInvoiceId?: string,
): Promise<InvoiceDetails> {
  const invoiceId = selection.kind === 'existing' ? selection.invoice.id : selection.body.id;
  try {
    return selection.kind === 'existing'
      ? await request<InvoiceDetails>('/api/v1/invoices/add-service-charge', session, {
          invoice_id: invoiceId,
          service_charge_id: serviceChargeId,
          expected_revision: selection.invoice.revision,
          ...(previousInvoiceId ? { previous_invoice_id: previousInvoiceId } : {}),
        })
      : await request<InvoiceDetails>('/api/v1/invoices/save', session, {
          ...selection.body,
          service_charge_ids: [serviceChargeId],
          expected_revision: 0,
          ...(previousInvoiceId ? { previous_invoice_id: previousInvoiceId } : {}),
        });
  } catch (error) {
    // A response may be lost after commit; checking the same ID makes a retry safe.
    try {
      const saved = await request<InvoiceDetails>('/api/v1/invoices/get', session, {
        id: invoiceId,
      });
      if (!saved.invoice.archived && saved.charges.some((charge) => charge.id === serviceChargeId))
        return saved;
    } catch {
      /* Preserve the original error. */
    }
    throw error;
  }
}

export function ServiceInvoiceActions({
  service,
  session,
  selection,
  onSelect,
  compact = false,
  current,
  initialMode,
  onClose,
  summary,
}: {
  initialMode?: 'new' | 'existing';
  onClose?: () => void;
  summary?: ReactNode;
  service: ServiceContext;
  session: Session;
  compact?: boolean;
  current?: { invoice_id: string; number: string; invoice_date: string };
  selection?: ServiceInvoiceSelection;
  onSelect: (selection: ServiceInvoiceSelection | undefined) => Promise<void> | void;
}) {
  const { translate } = useLocalization();
  const [open, setOpen] = useState(!!initialMode);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const direction = service.direction === 'income' ? 'outgoing' : 'incoming';
  const choose = async (value: ServiceInvoiceSelection | undefined) => {
    if (busy) return;
    setBusy(true);
    setFailure('');
    try {
      await onSelect(value);
      setOpen(false);
      onClose?.();
    } catch (error) {
      setFailure(error instanceof Error ? error.message : String(error));
    } finally {
      setBusy(false);
    }
  };
  const create = useRelatedCreate({
    kind: 'invoice',
    defaults: {
      direction,
      counterparty_id: service.counterparty_id,
      counterparty_name: service.counterparty_name,
      contract_id: service.contract_id,
      contract_number: service.contract_number,
      currency: service.invoice_currency,
    },
    scope: JSON.stringify([
      session.subject,
      session.organization_id,
      direction,
      service.counterparty_id,
      service.contract_id,
      service.invoice_currency,
    ]),
    enabled:
      !busy &&
      !!service.counterparty_id &&
      !!service.invoice_currency &&
      session.permissions.includes('finance.write_invoices') &&
      session.permissions.includes('finance.read_invoices'),
    onCreated(invoice) {
      void choose({ kind: 'existing', invoice });
    },
  });
  const started = useRef(false);
  useEffect(() => {
    if (initialMode === 'new' && !started.current && create) {
      started.current = true;
      create();
    }
  }, [initialMode, create]);
  const load = useCallback(
    async (query: string, signal: AbortSignal) => {
      const result = await request<{ items: InvoiceRegistryItem[]; next_cursor?: unknown }>(
        '/api/v1/invoices/list',
        session,
        {
          direction,
          sort: 'invoice_date',
          descending: true,
          limit: 100,
          filters: [
            { field: 'archived', values: ['false'] },
            { field: 'counterparty_id', values: [service.counterparty_id] },
            { field: 'currency', values: [service.invoice_currency] },
            service.contract_id
              ? { field: 'contract_id', values: [service.contract_id] }
              : { field: 'contract_id', empty: true },
            ...(query ? [{ field: 'number', query }] : []),
          ],
        },
        signal,
      );
      return { items: result.items.map((item) => item.invoice), hasMore: !!result.next_cursor };
    },
    [session, direction, service.counterparty_id, service.invoice_currency, service.contract_id],
  );
  const chosen =
    selection?.kind === 'existing'
      ? selection.invoice
      : selection?.kind === 'new'
        ? selection.body
        : current;
  const label = (invoice: { number: string | null; invoice_date: string }) =>
    translate(invoice.number ? 'finance.invoiceNumberDate' : 'finance.invoiceAutoDate', {
      number: invoice.number ?? '',
      date: CalendarDate.fromISO(invoice.invoice_date).toDisplayString(),
    });
  const field = (
    <LookupField<Invoice>
      label={translate('registryField.invoice_number')}
      value={chosen ? ('id' in chosen ? chosen.id : chosen.invoice_id) : ''}
      selectedLabel={chosen ? label(chosen) : ''}
      placeholder={translate('finance.chooseInvoice')}
      load={load}
      itemLabel={label}
      itemDescription={(invoice) =>
        `${invoice.own_company_name} · ${formatFixed(invoice.total_minor, 2)} ${invoice.currency}`
      }
      disabled={busy || !service.counterparty_id || !service.invoice_currency}
      error={failure}
      clearLabel={current ? translate('finance.unlink') : undefined}
      onChange={(invoice) => {
        void choose(invoice ? { kind: 'existing', invoice } : undefined);
      }}
      onCreate={create}
      createLabel={translate('related.createInvoice')}
    />
  );
  return (
    <div
      className={
        initialMode ? 'bulk-invoice-picker' : `service-invoice-field${compact ? ' compact' : ''}`
      }
    >
      {!initialMode && field}
      {selection?.kind === 'new' && (
        <span className="service-secondary">{translate('finance.createOnChargeSave')}</span>
      )}
      {initialMode && open && (
        <Dialog
          title={translate('finance.selectedChargesInvoice')}
          onClose={() => {
            if (!busy) {
              setOpen(false);
              onClose?.();
            }
          }}
          className="service-invoice-dialog"
        >
          {summary}
          {field}
        </Dialog>
      )}
    </div>
  );
}
