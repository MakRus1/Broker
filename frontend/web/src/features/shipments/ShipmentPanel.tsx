import { useLocalization } from '../localization/Localization';
import { Select } from '../../shared/ui/Select';
import { PlanQuantityForm, IncompleteContainers } from './ContainerPlan';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import { ApiError, request } from '../../shared/api/http';
import { Commands } from '../../shared/api/commands';
import { operationStorageKey, readOperationIds } from './operations';
import { ownership } from '../routes/logisticsLabels';
import { type Item, type ContainerFact, type Operation, type Shipment } from './types';
import { type Transportation } from '../transportations/types';
import { type Session } from '../../shared/api/session';
import { CardNavigation } from '../../shared/ui/CardNavigation';
import { Dialog, useDiscardConfirmation } from '../../shared/ui/Dialog';
import { TransportationTable } from '../transportations/TransportationTable';

const operationStatusKeys = {
  pending: 'shipment.creating',
  succeeded: 'shipment.created',
  rejected: 'shipment.rejected',
} as const;

type Props = {
  active?: boolean;
  initial: Shipment;
  containerTypes?: string[];
  embedded?: boolean;
  section?: 'items' | 'transportations';
  customerName: string;
  session: Session;
  onClose: () => void;
  onError: (error: unknown) => void;
  onUpdate: (shipment: Shipment) => void;
};
export function ShipmentPanel({
  active = true,
  customerName,
  containerTypes = ['20DC', '40DC', '40HC'],
  embedded = false,
  section,
  initial,
  session,
  onClose,
  onError,
  onUpdate,
}: Props) {
  const { translate } = useLocalization();
  const [shipment, setShipment] = useState(initial);
  const updateCallback = useRef(onUpdate);
  updateCallback.current = onUpdate;
  useEffect(() => {
    if (!embedded) document.title = `${shipment.number} · Broker`;
  }, [shipment.number]);
  const [items, setItems] = useState<Item[]>([]);
  const [unplanned, setUnplanned] = useState<ContainerFact[]>([]);
  const [editPlan, setEditPlan] = useState<Item | null>(null);
  const [transportations, setTransportations] = useState<Transportation[]>([]);
  const [tab, setTab] = useState<'items' | 'transportations'>('items');
  useEffect(() => {
    if (section) setTab(section);
  }, [section]);
  const [form, setForm] = useState<'item' | Item | null>(null);
  const [operations, setOperations] = useState<Operation[]>([]);
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [moreItems, setMoreItems] = useState(false);
  const [moreTransportations, setMoreTransportations] = useState(false);
  const panel = useRef<HTMLElement>(null);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const commands = useRef(new Commands());
  const storageKey = operationStorageKey(session.actor, initial.id);
  const [ids, setIds] = useState(() => readOperationIds(storageKey));
  const write = session.permissions.includes('shipments.write');
  const canReadTransportations = session.permissions.includes('transportations.read');
  const refresh = useCallback(
    async (signal?: AbortSignal) => {
      const [selected, positions, deliveries] = await Promise.all([
        request<{ shipment: Shipment }>(
          '/api/v1/shipments/get',
          session,
          { shipment_id: initial.id },
          signal,
        ),
        request<{ items: Item[]; unplanned_items: ContainerFact[] }>(
          '/api/v1/shipment-items/list',
          session,
          { shipment_id: initial.id, limit: 100 },
          signal,
        ),
        canReadTransportations
          ? request<{ items: Transportation[] }>(
              '/api/v1/transportations/list',
              session,
              { filters: [{ field: 'shipment_id', values: [initial.id] }], limit: 50 },
              signal,
            )
          : Promise.resolve({ items: [] }),
      ]);
      if (signal?.aborted) return;
      setShipment(selected.shipment);
      updateCallback.current(selected.shipment);
      setItems(positions.items);
      setUnplanned(positions.unplanned_items);
      setTransportations(deliveries.items);
      setMoreItems(positions.items.length === 100);
      setMoreTransportations(deliveries.items.length === 50);
      setFailure('');
    },
    [session, initial.id, canReadTransportations],
  );
  useEffect(() => {
    if (!active) return;
    const controller = new AbortController();
    void refresh(controller.signal)
      .catch((value) => {
        if (!controller.signal.aborted) {
          setFailure(value.message);
          onError(value);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [refresh, onError, active]);
  useEffect(() => {
    if (!active || !ids.length) return;
    const controller = new AbortController();
    let timer: ReturnType<typeof setTimeout>;
    let warned = false;
    async function poll() {
      try {
        const values = await Promise.all(
          ids.map(async (id) => {
            try {
              return (
                await request<{ operation: Operation }>(
                  '/api/v1/transportation-operations/get',
                  session,
                  { operation_id: id },
                  controller.signal,
                )
              ).operation;
            } catch (value) {
              if (value instanceof ApiError && value.status === 404) return null;
              throw value;
            }
          }),
        );
        if (controller.signal.aborted) return;
        const found = values.filter((value): value is Operation => value !== null);
        setOperations(found);
        warned = false;
        await refresh(controller.signal);
        if (found.some((value) => value.status === 'pending')) timer = setTimeout(poll, 2000);
      } catch (value) {
        if (controller.signal.aborted) return;
        if (!warned) {
          onError(value);
          warned = true;
        }
        timer = setTimeout(poll, 5000);
      }
    }
    if (ids.length) void poll();
    return () => {
      controller.abort();
      clearTimeout(timer);
    };
  }, [ids, session, refresh, onError, active]);
  function close() {
    if (panel.current?.querySelector('form[data-busy="true"]')) return;
    confirmDiscard(!!panel.current?.querySelector('form[data-dirty="true"]'), onClose);
  }
  async function nextPage(kind: 'items' | 'transportations') {
    setLoading(true);
    setFailure('');
    try {
      if (kind === 'items') {
        const result = await request<{ items: Item[] }>('/api/v1/shipment-items/list', session, {
          shipment_id: shipment.id,
          after: items.at(-1)!.id,
          limit: 100,
        });
        setItems((old) => [...old, ...result.items]);
        setMoreItems(result.items.length === 100);
      } else {
        const result = await request<{ items: Transportation[] }>(
          '/api/v1/transportations/list',
          session,
          {
            filters: [{ field: 'shipment_id', values: [shipment.id] }],
            after: transportations.at(-1)!.id,
            limit: 50,
          },
        );
        setTransportations((old) => [...old, ...result.items]);
        setMoreTransportations(result.items.length === 50);
      }
    } catch (value) {
      setFailure(value instanceof Error ? value.message : translate('shipment.loadRecordsFailed'));
      onError(value);
    } finally {
      setLoading(false);
    }
  }
  async function submit(body: object) {
    if (form === 'item')
      await commands.current.send('/api/v1/shipment-items/add', session, {
        ...body,
        shipment_id: shipment.id,
        expected_revision: shipment.revision,
      });
    else if (form) {
      const result = await commands.current.send<{ operation: Operation }>(
        '/api/v1/transportation-operations/create',
        session,
        { ...body, item_id: form.id, expected_revision: form.revision },
      );
      const updated = [
        ...ids.filter((id) => id !== result.operation.id),
        result.operation.id,
      ].slice(-100);
      setIds(updated);
      try {
        localStorage.setItem(storageKey, JSON.stringify(updated));
      } catch {
        /* Без хранилища статус доступен до закрытия страницы. */
      }
      setOperations((old) => [
        ...old.filter((value) => value.id !== result.operation.id),
        result.operation,
      ]);
    }
    setForm(null);
    await refresh();
  }
  return (
    <div className={`shipment-page${embedded ? ' shipment-embedded' : ''}`}>
      <section ref={panel} aria-labelledby="shipment-title" className="shipment-card">
        {!embedded && (
          <>
            <div className="panel-heading">
              <div>
                <p className="eyebrow">{translate('shipment.eyebrow')}</p>
                <h2 id="shipment-title">{shipment.number}</h2>
              </div>
              <button aria-label={translate('shipment.backToShipments')} onClick={close}>
                {translate('shipment.backToList')}
              </button>
            </div>
            <div className="shipment-summary">
              <div>
                <span>{translate('registryValue.roles.customer')}</span>
                <strong>{customerName}</strong>
              </div>
              <div>
                <span>{translate('shipment.note')}</span>
                <strong>{shipment.reference || translate('shipment.notSpecified')}</strong>
              </div>
              <div>
                <span>{translate('shipment.revisionLabel')}</span>
                <strong>{shipment.revision}</strong>
              </div>
            </div>
            <CardNavigation
              disabled={loading}
              onRefresh={async () => {
                setLoading(true);
                try {
                  await refresh();
                } catch (error) {
                  onError(error);
                } finally {
                  setLoading(false);
                }
              }}
            >
              <button aria-pressed={tab === 'items'} onClick={() => setTab('items')}>
                {translate('shipment.items')}
              </button>
              <button
                aria-pressed={tab === 'transportations'}
                onClick={() => setTab('transportations')}
              >
                {translate('nav.transportations')}
              </button>
            </CardNavigation>
          </>
        )}
        <div className="panel-content" aria-busy={loading}>
          {failure && (
            <p className="error" role="alert">
              {failure}
            </p>
          )}
          {tab === 'items' ? (
            <>
              <div className="section-heading">
                <div>
                  <h3>{translate('shipment.plannedContainers')}</h3>
                  <p>{translate('shipment.planMatchingHint')}</p>
                </div>
                {write && (
                  <button
                    className="primary"
                    disabled={loading || Boolean(failure)}
                    onClick={() => setForm('item')}
                  >
                    {translate('shipment.addItemButton')}
                  </button>
                )}
              </div>
              {items.map((item) => (
                <article className="item-card" key={item.id}>
                  <div className="item-title">
                    <strong>{item.container_type_code}</strong>
                    <span className="badge">{ownership[item.container_ownership]}</span>
                  </div>
                  <div className="quantities">
                    <div>
                      <span>{translate('shipment.planned')}</span>
                      <strong>{item.planned_quantity}</strong>
                    </div>
                    <div>
                      <span>{translate('shipment.created')}</span>
                      <strong>{item.created_quantity}</strong>
                    </div>
                    {item.pending_quantity > 0 && (
                      <div>
                        <span>{translate('shipment.creating')}</span>
                        <strong>{item.pending_quantity}</strong>
                      </div>
                    )}
                    <div>
                      <span>{translate('shipment.remaining')}</span>
                      <strong>{item.available_quantity}</strong>
                    </div>
                  </div>
                  {item.excess_quantity > 0 && (
                    <p className="plan-excess" role="status">
                      {translate('shipment.planExcess', { count: item.excess_quantity })}
                    </p>
                  )}
                  {write && (
                    <button disabled={loading} onClick={() => setEditPlan(item)}>
                      {translate('shipment.editPlan')}
                    </button>
                  )}
                  {write && (
                    <button disabled={loading} onClick={() => setForm(item)}>
                      {translate('shipment.createTransportationsArrow')}
                    </button>
                  )}
                </article>
              ))}
              {!items.length && !loading && !failure && (
                <div className="empty">
                  <h3>{translate('shipment.firstItem')}</h3>
                  <p>{translate('shipment.firstItemHint')}</p>
                </div>
              )}
              {unplanned.length > 0 && (
                <section className="unplanned-containers">
                  <h3>{translate('shipment.unplanned')}</h3>
                  {unplanned
                    .filter((value) => value.container_type_code && value.container_ownership)
                    .map((value) => (
                      <p key={`${value.container_type_code}-${value.container_ownership}`}>
                        {value.container_type_code} · {value.container_ownership}: {value.quantity}{' '}
                        {translate('shipment.transportationShort')}
                      </p>
                    ))}
                  {unplanned.some(
                    (value) => !value.container_type_code || !value.container_ownership,
                  ) && (
                    <>
                      <p>
                        {translate('shipment.incompletePlan', {
                          count: unplanned
                            .filter(
                              (value) => !value.container_type_code || !value.container_ownership,
                            )
                            .reduce((sum, value) => sum + value.quantity, 0),
                        })}
                      </p>
                      {canReadTransportations && (
                        <IncompleteContainers
                          active={active}
                          shipmentId={shipment.id}
                          session={session}
                          onError={onError}
                        />
                      )}
                    </>
                  )}
                </section>
              )}
              <AutoLoadMore
                enabled={moreItems}
                loading={loading}
                failed={Boolean(failure)}
                onLoad={() => nextPage('items')}
              />
              {operations.length > 0 && (
                <section className="operations">
                  <h3>{translate('shipment.creationOperations')}</h3>
                  <p className="hint">{translate('shipment.operationsHint')}</p>
                  {operations.map((value) => (
                    <div className="operation" key={value.id}>
                      <div>
                        <strong>
                          {value.quantity} {translate('shipment.transportationShort')}
                        </strong>
                        <small className="mono">{value.id}</small>
                        {value.rejection_code && <small>{value.rejection_code}</small>}
                      </div>
                      <span className={`badge ${value.status}`}>
                        {translate(operationStatusKeys[value.status])}
                      </span>
                    </div>
                  ))}
                </section>
              )}
            </>
          ) : (
            <>
              {!canReadTransportations ? (
                <p>{translate('shipment.noTransportationAccess')}</p>
              ) : (
                <>
                  {session.permissions.includes('transportations.write') && (
                    <div className="section-heading">
                      <h3>{translate('shipment.shipmentTransportations')}</h3>
                      <a className="primary" href={`#/transportations/new/${shipment.id}`}>
                        {translate('shipment.createTransportationButton')}
                      </a>
                    </div>
                  )}
                  <TransportationTable items={transportations} />
                  {!transportations.length && (
                    <div className="empty">
                      <h3>{translate('shipment.noTransportations')}</h3>
                      <p>{translate('shipment.noTransportationsHint')}</p>
                    </div>
                  )}
                  <AutoLoadMore
                    enabled={moreTransportations}
                    loading={loading}
                    failed={Boolean(failure)}
                    onLoad={() => nextPage('transportations')}
                  />
                </>
              )}
            </>
          )}
        </div>
        {form && (
          <Dialog
            title={
              form === 'item'
                ? translate('shipment.addItem')
                : translate('shipment.createTransportations')
            }
            onClose={() => setForm(null)}
          >
            <PositionForm
              key={form === 'item' ? 'item' : form.id}
              item={form === 'item' ? undefined : form}
              containerTypes={containerTypes}
              onSubmit={submit}
              onError={onError}
            />
          </Dialog>
        )}
      </section>
      {editPlan && (
        <Dialog title={translate('shipment.editPlan')} onClose={() => setEditPlan(null)}>
          <PlanQuantityForm
            item={editPlan}
            onError={onError}
            onSubmit={async (quantity) => {
              await request('/api/v1/shipment-items/update', session, {
                item_id: editPlan.id,
                expected_revision: editPlan.revision,
                planned_quantity: quantity,
              });
              setEditPlan(null);
              await refresh();
            }}
          />
        </Dialog>
      )}
      {confirmation}
    </div>
  );
}
function PositionForm({
  item,
  containerTypes,
  onSubmit,
  onError,
}: {
  item?: Item;
  containerTypes: string[];
  onSubmit: (body: object) => Promise<void>;
  onError: (value: unknown) => void;
}) {
  const { translate } = useLocalization();
  const defaultCode = containerTypes.includes('40HC') ? '40HC' : (containerTypes[0] ?? '');
  const [code, setCode] = useState(defaultCode);
  const [owner, setOwner] = useState('COC');
  const [quantity, setQuantity] = useState('1');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const dirty = quantity !== '1' || (!item && (code !== defaultCode || owner !== 'COC'));
  async function submit(event: FormEvent) {
    event.preventDefault();
    setBusy(true);
    setFailure('');
    try {
      await onSubmit(
        item
          ? { quantity: Number(quantity) }
          : {
              container_type_code: code.trim().toUpperCase(),
              container_ownership: owner,
              planned_quantity: Number(quantity),
            },
      );
    } catch (value) {
      setFailure(value instanceof Error ? value.message : translate('shipment.saveError'));
      onError(value);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form onSubmit={submit} data-dirty={dirty} data-busy={busy}>
      {item ? (
        <p className="hint">
          {translate('shipment.availableUnits', {
            type: item.container_type_code,
            count: item.available_quantity,
          })}
        </p>
      ) : (
        <>
          <Select
            label={translate('registryField.container_type')}
            disabled={busy}
            value={code}
            onChange={setCode}
            options={containerTypes.map((value) => ({ value, label: value }))}
          />
          <Select
            label={translate('registryField.container_ownership')}
            disabled={busy}
            value={owner}
            onChange={setOwner}
            options={[
              { value: 'COC', label: 'COC' },
              { value: 'SOC', label: 'SOC' },
            ]}
          />
        </>
      )}
      <label>
        {item ? translate('shipment.createQuantity') : translate('shipment.plannedQuantity')}
        <input
          disabled={busy}
          autoFocus={Boolean(item)}
          required
          type="number"
          min={1}
          step={1}
          max={item ? 100 : 10000}
          value={quantity}
          onChange={(event) => setQuantity(event.target.value)}
        />
      </label>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <button className="primary" disabled={busy}>
        {busy
          ? translate('shipment.saving')
          : item
            ? translate('shipment.createTransportations')
            : translate('shipment.addItem')}
      </button>
    </form>
  );
}
