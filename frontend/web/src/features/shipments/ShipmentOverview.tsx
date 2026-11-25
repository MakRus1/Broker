import { useCallback, useState } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useActivationLoad } from '../../shared/api/useActivationLoad';
import {
  formatFixed,
  type CurrencyTotal,
  type ReportingSummary,
  type ServiceChargeSummary,
} from '../finance/services/serviceCharges';
import { useLocalization } from '../localization/Localization';
import type { Item, Shipment } from './types';

const countLimit = 51;

type TransportationFacts =
  { planned: number; created: number; pending: number } | { count: number };

async function loadTransportationFacts(
  shipmentId: string,
  plannedContainers: boolean,
  session: Session,
  signal: AbortSignal,
): Promise<TransportationFacts> {
  if (plannedContainers) {
    const totals = { planned: 0, created: 0, pending: 0 };
    let after: string | undefined;
    for (;;) {
      const { items } = await request<{ items: Item[] }>(
        '/api/v1/shipment-items/list',
        session,
        { shipment_id: shipmentId, limit: 100, after },
        signal,
      );
      for (const item of items) {
        totals.planned += item.planned_quantity;
        totals.created += item.created_quantity;
        totals.pending += item.pending_quantity;
      }
      if (items.length < 100) break;
      after = items[99].id;
    }
    if (totals.planned > 0) return totals;
  }
  const { items } = await request<{ items: { id: string }[] }>(
    '/api/v1/transportations/list',
    session,
    { filters: [{ field: 'shipment_id', values: [shipmentId] }], limit: countLimit },
    signal,
  );
  return { count: items.length };
}

export function ShipmentOverview({
  active,
  shipment,
  plannedContainers,
  session,
  onOpen,
}: {
  active: boolean;
  shipment: Shipment;
  plannedContainers?: boolean;
  session: Session;
  onOpen: (tab: 'transportations' | 'route' | 'finance') => void;
}) {
  const { translate } = useLocalization();
  const [transport, setTransport] = useState<TransportationFacts | null>();
  const [finance, setFinance] = useState<CurrencyTotal[] | null>();
  const shipmentId = shipment.id;
  const canReadTransport = session.permissions.includes('transportations.read');
  const canReadFinance = session.permissions.includes('finance.read_service_charges');
  const loadTransport = useCallback(
    async (signal: AbortSignal) => {
      const facts = await loadTransportationFacts(
        shipmentId,
        plannedContainers === true,
        session,
        signal,
      );
      if (!signal.aborted) setTransport(facts);
    },
    [shipmentId, plannedContainers, session],
  );
  const loadFinance = useCallback(
    async (signal: AbortSignal) => {
      const { summary, reporting } = await request<{
        summary: ServiceChargeSummary;
        reporting?: ReportingSummary;
      }>(
        '/api/v1/service-charges/list',
        session,
        {
          scope: { shipment_id: shipmentId, transportation_id: null },
          limit: 1,
          after: null,
          include_archived: false,
        },
        signal,
      );
      if (!signal.aborted)
        setFinance(
          (reporting && !reporting.missing_rates.length ? reporting.summary : summary).combined,
        );
    },
    [shipmentId, session],
  );
  useActivationLoad(
    active && canReadTransport && plannedContainers !== undefined,
    loadTransport,
    () => setTransport(null),
  );
  useActivationLoad(active && canReadFinance, loadFinance, () => setFinance(null));

  const documents = (shipment.route_segments ?? []).reduce(
    (count, stage) =>
      count + (stage.sea?.documents?.length ?? 0) + (stage.air?.documents?.length ?? 0),
    0,
  );
  const unavailable = translate('shipment.overviewUnavailable');
  const loading = translate('common.loading');
  const transportText =
    transport === null
      ? unavailable
      : transport === undefined
        ? loading
        : 'planned' in transport
          ? [
              translate('shipment.overviewContainerPlan', {
                created: transport.created,
                planned: transport.planned,
              }),
              transport.pending > 0 &&
                translate('shipment.overviewPending', { count: transport.pending }),
            ]
              .filter(Boolean)
              .join(' · ')
          : transport.count === 0
            ? translate('shipment.noTransportations')
            : translate('shipment.overviewTransportCount', {
                count: transport.count >= countLimit ? `${countLimit - 1}+` : transport.count,
              });

  return (
    <section className="shipment-overview-facts">
      <h2>{translate('shipment.overviewTitle')}</h2>
      <dl>
        {canReadTransport && (
          <div>
            <dt>
              <button
                type="button"
                className="link-button"
                onClick={() => onOpen('transportations')}
              >
                {translate('nav.transportations')}
              </button>
            </dt>
            <dd>{transportText}</dd>
          </div>
        )}
        <div>
          <dt>
            <button type="button" className="link-button" onClick={() => onOpen('route')}>
              {translate('shipment.overviewDocuments')}
            </button>
          </dt>
          <dd>
            {documents
              ? translate('shipment.overviewDocumentCount', { count: documents })
              : translate('shipment.overviewNoDocuments')}
          </dd>
        </div>
        {canReadFinance && (
          <div>
            <dt>
              <button type="button" className="link-button" onClick={() => onOpen('finance')}>
                {translate('home.finance')}
              </button>
            </dt>
            <dd>
              {finance === null
                ? unavailable
                : finance === undefined
                  ? loading
                  : finance.length === 0
                    ? translate('finance.noServices')
                    : finance.map((total) => (
                        <span key={total.currency}>
                          {translate('shipment.overviewServiceTotal', {
                            income: formatFixed(total.income_minor, 2),
                            expense: formatFixed(total.expense_minor, 2),
                            currency: total.currency,
                          })}
                        </span>
                      ))}
            </dd>
          </div>
        )}
      </dl>
    </section>
  );
}
