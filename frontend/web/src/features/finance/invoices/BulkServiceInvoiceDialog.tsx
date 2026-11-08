import { useLocalization } from '../../localization/Localization';
import { useNotifications } from '../../notifications/Notifications';
import type { Session } from '../../../shared/api/session';
import { formatFixed } from '../services/serviceCharges';
import { attachBulkInvoice, type SelectableCharge } from './bulkInvoices';
import { ServiceInvoiceActions } from './ServiceInvoiceActions';

export type BulkServiceInvoice = { mode: 'new' | 'existing'; items: SelectableCharge[] };

export function BulkServiceInvoiceDialog({
  selection,
  session,
  onClose,
  onSaved,
}: {
  selection: BulkServiceInvoice;
  session: Session;
  onClose: () => void;
  onSaved: () => void;
}) {
  const { translate } = useLocalization();
  const notifications = useNotifications();
  const { items, mode } = selection;
  const service = items[0];
  return (
    <ServiceInvoiceActions
      service={service}
      session={session}
      initialMode={mode}
      onClose={onClose}
      summary={
        <p className="hint">
          {translate('bulk.chargeCount', { count: items.length })} · {service.counterparty_name} ·{' '}
          {translate('registryField.total_minor')}:{' '}
          {formatFixed(
            items.reduce((sum, item) => sum + BigInt(item.invoice_total_minor), 0n),
            2,
          )}{' '}
          {service.invoice_currency}
        </p>
      }
      onSelect={async (value) => {
        if (!value) return;
        const saved = await attachBulkInvoice(
          items.map((item) => item.id),
          value,
          session,
        );
        notifications.publish({
          level: 'success',
          message: translate('bulk.invoiceSaved', {
            number: saved.invoice.number,
            count: items.length,
          }),
          route: `/${saved.invoice.direction}-invoices/${saved.invoice.id}`,
        });
        onSaved();
      }}
    />
  );
}
