import { useEffect, useState } from 'react';
import { request, type Item, type Session, type Transportation } from './api';
import { TransportationTable } from './components';
import { AutoLoadMore } from './AutoLoadMore';
export function PlanQuantityForm({
  item,
  onSubmit,
  onError,
}: {
  item: Item;
  onSubmit: (quantity: number) => Promise<void>;
  onError: (error: unknown) => void;
}) {
  const [quantity, setQuantity] = useState(String(item.planned_quantity));
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  return (
    <form
      data-dirty={quantity !== String(item.planned_quantity)}
      data-busy={busy}
      onSubmit={async (event) => {
        event.preventDefault();
        if (busy) return;
        setBusy(true);
        setFailure('');
        try {
          await onSubmit(Number(quantity));
        } catch (error) {
          setFailure(error instanceof Error ? error.message : 'Не удалось изменить план');
          onError(error);
        } finally {
          setBusy(false);
        }
      }}
    >
      <p>
        {item.container_type_code} · {item.container_ownership}
      </p>
      <label>
        По плану
        <input
          autoFocus
          required
          type="number"
          min={0}
          max={2147483647}
          step={1}
          value={quantity}
          disabled={busy}
          onChange={(event) => setQuantity(event.target.value)}
        />
      </label>
      {Number(quantity) < item.created_quantity && (
        <p className="plan-excess">
          Превышение плана на {item.created_quantity - Number(quantity)}. Сохранение разрешено.
        </p>
      )}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <button className="primary" disabled={busy}>
        {busy ? 'Сохраняем…' : 'Сохранить'}
      </button>
    </form>
  );
}
export function IncompleteContainers({
  shipmentId,
  session,
  onError,
  active,
}: {
  active: boolean;
  shipmentId: string;
  session: Session;
  onError: (error: unknown) => void;
}) {
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<Transportation[]>([]);
  const [more, setMore] = useState(true);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    if (!open || !active) return;
    const controller = new AbortController();
    void load(undefined, controller.signal);
    return () => controller.abort();
  }, [open, active, shipmentId, session]);
  async function load(after?: string, signal?: AbortSignal) {
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ items: Transportation[] }>(
        '/api/v1/transportations/list',
        session,
        {
          filters: [
            { field: 'shipment_id', values: [shipmentId] },
            { field: 'incomplete_container', values: ['true'] },
          ],
          limit: 50,
          ...(after ? { after } : {}),
        },
        signal,
      );
      if (signal?.aborted) return;
      setItems((old) => (after ? [...old, ...result.items] : result.items));
      setMore(result.items.length === 50);
    } catch (error) {
      if (signal?.aborted) return;
      setFailure('Не удалось загрузить перевозки');
      onError(error);
    } finally {
      if (!signal?.aborted) setBusy(false);
    }
  }
  return (
    <div>
      <button
        aria-expanded={open}
        onClick={() => {
          setOpen(!open);
        }}
      >
        {open ? 'Скрыть' : 'Показать перевозки'}
      </button>
      {open && (
        <>
          {failure && (
            <p className="error" role="alert">
              {failure} <button onClick={() => void load(items.at(-1)?.id)}>Повторить</button>
            </p>
          )}
          <TransportationTable items={items} />
          <AutoLoadMore
            enabled={more && items.length > 0}
            loading={busy}
            failed={Boolean(failure)}
            onLoad={() => load(items.at(-1)?.id)}
          />
        </>
      )}
    </div>
  );
}
