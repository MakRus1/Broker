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

type ContainerPlanFacts = { planned: number; created: number; pending: number; remaining: number };

async function loadContainerPlan(
  shipmentId: string,
  session: Session,
  signal: AbortSignal,
  loadFirstItemPage: () => Promise<{ items: Item[] }>,
): Promise<ContainerPlanFacts | null> {
  const totals = { planned: 0, created: 0, pending: 0, remaining: 0 };
  let after: string | undefined;
  for (;;) {
    const { items } = after
      ? await request<{ items: Item[] }>(
          '/api/v1/shipment-items/list',
          session,
          { shipment_id: shipmentId, limit: 100, after },
          signal,
        )
      : await loadFirstItemPage();
    signal.throwIfAborted();
    for (const item of items) {
      totals.planned += item.planned_quantity;
      totals.created += item.created_quantity;
      totals.pending += item.pending_quantity;
      totals.remaining += Math.max(
        0,
        item.planned_quantity - item.created_quantity - item.pending_quantity,
      );
    }
    if (items.length < 100) break;
    after = items[99].id;
  }
  return totals.planned > 0 ? totals : null;
}

export function ShipmentOverview({
  active,
  shipment,
  loadFirstItemPage,
  plannedContainers,
  transportationCount,
  session,
  onOpen,
}: {
  active: boolean;
  shipment: Shipment;
  loadFirstItemPage: () => Promise<{ items: Item[] }>;
  plannedContainers?: boolean;
  transportationCount?: number | null;
  session: Session;
  onOpen: (tab: 'transportations' | 'route' | 'finance') => void;
}) {
  const { translate } = useLocalization();
  const [plan, setPlan] = useState<ContainerPlanFacts | null | 'unavailable'>();
  const [fallbackCount, setFallbackCount] = useState<number | null>();
  const [finance, setFinance] = useState<CurrencyTotal[] | null>();
  const shipmentId = shipment.id;
  const canReadTransport = session.permissions.includes('transportations.read');
  const canReadFinance = session.permissions.includes('finance.read_service_charges');
  const loadPlan = useCallback(
    async (signal: AbortSignal) => {
      const facts = await loadContainerPlan(shipmentId, session, signal, loadFirstItemPage);
      if (!signal.aborted) setPlan(facts);
    },
    [shipmentId, session, loadFirstItemPage],
  );
  const loadFallbackCount = useCallback(
    async (signal: AbortSignal) => {
      const { items } = await request<{ items: { id: string }[] }>(
        '/api/v1/transportations/list',
        session,
        { filters: [{ field: 'shipment_id', values: [shipmentId] }], limit: countLimit },
        signal,
      );
      if (!signal.aborted) setFallbackCount(items.length);
    },
    [shipmentId, session],
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
  const useCount = plannedContainers === false || (plannedContainers === true && plan === null);
  useActivationLoad(active && canReadTransport && plannedContainers === true, loadPlan, () =>
    setPlan('unavailable'),
  );
  useActivationLoad(
    active && canReadTransport && useCount && transportationCount === null,
    loadFallbackCount,
    () => setFallbackCount(null),
  );
  useActivationLoad(active && canReadFinance, loadFinance, () => setFinance(null));

  const stages = shipment.route_segments ?? [];
  const seaStages = stages.filter((stage) => stage.type === 'sea');
  const confirmedBookings = seaStages.filter((stage) => stage.sea?.booking?.status === 'confirmed');
  const documents = stages.reduce(
    (count, stage) =>
      count + (stage.sea?.documents?.length ?? 0) + (stage.air?.documents?.length ?? 0),
    0,
  );
  const unavailable = translate('shipment.overviewUnavailable');
  const loading = translate('common.loading');
  const count = transportationCount === null ? fallbackCount : transportationCount;
  let transportText = loading;
  if (plannedContainers === true && plan === 'unavailable') {
    transportText = unavailable;
  } else if (plannedContainers === true && typeof plan === 'object' && plan) {
    transportText = [
      translate('shipment.overviewContainerPlan', { created: plan.created, planned: plan.planned }),
      plan.pending > 0 && translate('shipment.overviewPending', { count: plan.pending }),
      plan.remaining > 0 && translate('shipment.overviewRemaining', { count: plan.remaining }),
    ]
      .filter(Boolean)
      .join(' · ');
  } else if (useCount && count !== undefined) {
    transportText =
      count === null
        ? unavailable
        : count === 0
          ? translate('shipment.noTransportations')
          : translate('shipment.overviewTransportCount', {
              count:
                transportationCount === null && count >= countLimit ? `${countLimit - 1}+` : count,
            });
  }

  return (
    <section className="shipment-overview-facts">
      <h2>{translate('shipment.overviewTitle')}</h2>
      <dl>
        <div>
          <dt>
            <button type="button" className="link-button" onClick={() => onOpen('route')}>
              {translate('shipment.routeTitle')}
            </button>
          </dt>
          <dd>
            {stages.length ? (
              <>
                <span>{translate('shipment.overviewRouteStages', { count: stages.length })}</span>
                {seaStages.length > 0 && (
                  <span>
                    {translate('shipment.overviewBookings', {
                      confirmed: confirmedBookings.length,
                      total: seaStages.length,
                    })}
                  </span>
                )}
              </>
            ) : (
              translate('shipment.overviewNoRoute')
            )}
          </dd>
        </div>
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
