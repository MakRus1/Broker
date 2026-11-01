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
  const [value, setValue] = useState<{ charge: ServiceCharge; invoice?: ServiceChargeInvoice }>();
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
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
        if (!charge) throw new Error('Услуга не найдена или недоступна');
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
  return (
    <section className="finance-section service-charges-page">
      {failure ? (
        <div className="error" role="alert">
          {failure} <button onClick={() => setVersion((old) => old + 1)}>Повторить</button>
        </div>
      ) : !value ? (
        <p role="status">Загружаем услугу…</p>
      ) : (
        <ServiceChargeEditor
          key={`${id}:${version}`}
          initial={value.charge}
          session={session}
          customers={customers}
          currentInvoice={value.invoice}
          readOnly={
            !session.permissions.includes('finance.write_service_charges') ||
            value.invoice?.read_only === true
          }
          onCancel={onClose}
          onSaved={() => {
            window.dispatchEvent(new Event('finance-charges-changed'));
            setVersion((old) => old + 1);
          }}
        />
      )}
    </section>
  );
}
