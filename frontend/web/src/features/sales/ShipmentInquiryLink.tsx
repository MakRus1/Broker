import { useEffect, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';
import type { Inquiry } from './types';

export function ShipmentInquiryLink({ shipment, session }: { shipment: string; session: Session }) {
  const { translate } = useLocalization();
  const [inquiry, setInquiry] = useState<Inquiry>();
  useEffect(() => {
    if (!session.permissions.includes('sales.read')) return;
    const controller = new AbortController();
    request<{ items: Inquiry[] }>(
      '/api/v1/sales/inquiries/list',
      session,
      { shipment_id: shipment, limit: 1 },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted) setInquiry(items[0]);
      })
      .catch(() => {});
    return () => controller.abort();
  }, [shipment, session]);
  return inquiry?.shipment_id === shipment && session.permissions.includes('sales.read') ? (
    <>
      {' · '}
      <a className="record-layout-reference" href={`#/inquiries/${inquiry.id}`}>
        {translate('sales.inquiry')} {inquiry.number}
      </a>
    </>
  ) : null;
}
