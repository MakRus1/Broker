import { RecordHeading } from '../../../shared/ui/record/RecordLayout';
import { CardNavigation } from '../../../shared/ui/CardNavigation';
import { FinancialHistory } from '../history/FinancialHistory';
import { ServiceChargeSummary } from './ServiceChargeSummary';
import { useLocalization } from '../../localization/Localization';
import { useEffect, useRef, useState } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import type { Counterparty } from '../../counterparties/CustomerSelect';
import type { ServiceChargeInvoice } from '../invoices/invoices';
import { ServiceChargeEditor } from './ServiceChargeEditor';
import type { ServiceCharge } from './serviceCharges';

export function openServiceCharge(charge: Pick<ServiceCharge, 'id'>) {
  location.hash = `/service-charges/${charge.id}`;
}

export function ServiceChargePage({
  id,
  session,
  customers,
  onTitle,
  onClose,
}: {
  id: string;
  session: Session;
  customers: Counterparty[];
  onTitle: (title: string) => void;
  onClose: () => void;
}) {
  const { translate } = useLocalization();
  const [value, setValue] = useState<{ charge: ServiceCharge; invoice?: ServiceChargeInvoice }>();
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const [editing, setEditing] = useState(false);
  const [historyOpen, setHistoryOpen] = useState(false);
  const title = useRef(onTitle);
  title.current = onTitle;
  useEffect(() => {
    const controller = new AbortController();
    setFailure('');
    setValue(undefined);
    request<{ items: ServiceCharge[]; invoices: ServiceChargeInvoice[] }>(
      '/api/v1/service-charges/registry',
      session,
      { filters: [{ field: 'id', values: [id] }], limit: 1 },
      controller.signal,
    )
      .then((result) => {
        if (controller.signal.aborted) return;
        const charge = result.items[0];
        if (!charge) throw new Error(translate('finance.serviceNotFound'));
        setValue({
          charge,
          invoice: result.invoices.find((item) => item.service_charge_id === id),
        });
        title.current(charge.label_name);
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [id, session, version]);
  const readOnly =
    !session.permissions.includes('finance.write_service_charges') ||
    value?.invoice?.read_only === true;
  return (
    <section className="finance-section surface record-workbench financial-record-workbench">
      {failure ? (
        <div className="error" role="alert">
          {failure}{' '}
          <button onClick={() => setVersion((old) => old + 1)}>{translate('filter.retry')}</button>
        </div>
      ) : !value ? (
        <p role="status">{translate('finance.loadingService')}</p>
      ) : (
        <>
          <RecordHeading
            kind={translate('entity.serviceCharge')}
            title={value.charge.label_name}
            status={translate(value.charge.archived ? 'filter.archived' : 'finance.activeService')}
            metadata={
              <>
                {translate('shipment.revision', { revision: value.charge.revision })}
                {value.invoice?.read_only && <> · {translate('finance.paidChargeReadonly')}</>}
              </>
            }
            mode={editing ? translate('record.editing') : undefined}
            actions={
              !editing && !readOnly ? (
                <button className="primary" onClick={() => setEditing(true)}>
                  {translate('shipment.edit')}
                </button>
              ) : undefined
            }
          />
          {!editing && (
            <CardNavigation className="record-layout-navigation" tabsClassName="record-layout-tabs">
              <button aria-pressed={!historyOpen} onClick={() => setHistoryOpen(false)}>
                {translate('shipment.details')}
              </button>
              <button aria-pressed={historyOpen} onClick={() => setHistoryOpen(true)}>
                {translate('finance.history')}
              </button>
            </CardNavigation>
          )}
          {editing ? (
            <ServiceChargeEditor
              key={`${id}:${version}`}
              initial={value.charge}
              session={session}
              customers={customers}
              currentInvoice={value.invoice}
              readOnly={readOnly}
              showHeading={false}
              showNavigation={false}
              onCancel={onClose}
              onSaved={() => {
                window.dispatchEvent(new Event('finance-charges-changed'));
                setEditing(false);
                setVersion((old) => old + 1);
              }}
            />
          ) : historyOpen ? (
            <FinancialHistory
              kind="service-charges"
              id={id}
              revision={value.charge.revision}
              session={session}
            />
          ) : (
            <ServiceChargeSummary charge={value.charge} invoice={value.invoice} />
          )}
        </>
      )}
    </section>
  );
}
