import { useCallback, useState } from 'react';
import { request } from '../../shared/api/http';
import { useActivationLoad } from '../../shared/api/useActivationLoad';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';

export function InquiryShipmentLink({
  id,
  session,
  active = true,
}: {
  id: string;
  session: Session;
  active?: boolean;
}) {
  const { translate } = useLocalization();
  const [shipment, setShipment] = useState<{ id: string; number: string }>();
  const canRead = session.permissions.includes('shipments.read');

  const load = useCallback(
    async (signal: AbortSignal) => {
      const { shipment } = await request<{ shipment: { number: string } }>(
        '/api/v1/shipments/get',
        session,
        { shipment_id: id },
        signal,
      );
      if (!signal.aborted) setShipment({ id, number: shipment.number });
    },
    [id, session],
  );
  useActivationLoad(active && canRead, load, () => {});

  if (!canRead) return <>{translate('field.shipment')}</>;
  return (
    <a className="record-layout-reference" href={`#/shipments/${id}`}>
      {shipment?.id === id
        ? `${translate('field.shipment')} ${shipment.number}`
        : translate('sales.openShipment')}
    </a>
  );
}
