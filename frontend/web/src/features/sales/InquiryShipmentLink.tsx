import { useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';

export function InquiryShipmentLink({ id, session }: { id: string; session: Session }) {
  const { translate } = useLocalization();
  const [shipment, setShipment] = useState<{ id: string; number: string }>();
  const canRead = session.permissions.includes('shipments.read');

  useEffect(() => {
    if (!canRead) return;
    const controller = new AbortController();
    request<{ shipment: { number: string } }>(
      '/api/v1/shipments/get',
      session,
      { shipment_id: id },
      controller.signal,
    )
      .then(({ shipment }) => {
        if (!controller.signal.aborted) setShipment({ id, number: shipment.number });
      })
      .catch(() => {});
    return () => controller.abort();
  }, [canRead, id, session]);

  if (!canRead) return <>{translate('field.shipment')}</>;
  return (
    <a className="record-layout-reference" href={`#/shipments/${id}`}>
      {shipment?.id === id
        ? `${translate('field.shipment')} ${shipment.number}`
        : translate('sales.openShipment')}
    </a>
  );
}
