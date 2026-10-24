import { containerNumberWarning } from './transportation';
import { useEffect, useState, type FormEvent } from 'react';
import { request, transportationTypes, type Session, type Transportation } from './api';
import { Select } from './Select';
import { useDiscardConfirmation } from './components';

type Props = {
  initial?: Transportation;
  shipmentId: string;
  session: Session;
  onSaved: (value: Transportation) => void;
  onCancel: () => void;
  onError: (error: unknown) => void;
};
export function TransportationDetailsForm({
  initial,
  shipmentId,
  session,
  onSaved,
  onCancel,
  onError,
}: Props) {
  const [containerTypes, setContainerTypes] = useState<string[]>([]);
  const [settingsFailure, setSettingsFailure] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    request<{ container_types: string[] }>(
      '/api/v1/shipments/settings',
      session,
      {},
      controller.signal,
    )
      .then((settings) => setContainerTypes(settings.container_types))
      .catch((error) => {
        if (!controller.signal.aborted) setSettingsFailure(error.message);
      });
    return () => controller.abort();
  }, [session]);
  const [id] = useState(() => crypto.randomUUID());
  const [type, setType] = useState<Transportation['type']>(initial?.type ?? 'container');
  const [hazardous, setHazardous] = useState(initial?.hazardous ?? false);
  const [containerNumber, setContainerNumber] = useState(initial?.container_number ?? '');
  const numberWarning = containerNumberWarning(containerNumber);
  const [containerType, setContainerType] = useState(initial?.container_type_code ?? '');
  const [ownership, setOwnership] = useState(initial?.container_ownership ?? '');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [saved, setSaved] = useState(false);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty =
    !saved &&
    (type !== (initial?.type ?? 'container') ||
      hazardous !== (initial?.hazardous ?? false) ||
      containerType !== (initial?.container_type_code ?? '') ||
      ownership !== (initial?.container_ownership ?? '') ||
      containerNumber !== (initial?.container_number ?? ''));
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (busy) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ transportation: Transportation }>(
        `/api/v1/transportations/${initial ? 'update' : 'create'}`,
        session,
        {
          ...(initial
            ? { transportation_id: initial.id, expected_revision: initial.revision }
            : { id, shipment_id: shipmentId }),
          type,
          hazardous,
          ...(type === 'container' && containerNumber.trim()
            ? { container_number: containerNumber.trim().toUpperCase() }
            : {}),
          ...(type === 'container' && containerType
            ? { container_type_code: containerType.trim().toUpperCase() }
            : {}),
          ...(type === 'container' && ownership ? { container_ownership: ownership } : {}),
        },
      );
      setSaved(true);
      onSaved(result.transportation);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить перевозку');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      <form className="transportation-editor" onSubmit={submit} data-dirty={dirty} data-busy={busy}>
        <fieldset disabled={busy}>
          <div className="shipment-fields">
            <Select
              label="Тип перевозки"
              value={type}
              options={Object.entries(transportationTypes).map(([value, label]) => ({
                value,
                label,
              }))}
              onChange={(value) => setType(value as Transportation['type'])}
            />
            <label className="transportation-danger">
              <input
                type="checkbox"
                checked={hazardous}
                onChange={(event) => setHazardous(event.target.checked)}
              />
              Опасный груз
            </label>
            {type === 'container' && (
              <>
                <label className="transportation-number-field">
                  Номер контейнера
                  <input
                    maxLength={64}
                    placeholder="CSQU3054383"
                    autoCapitalize="characters"
                    spellCheck={false}
                    value={containerNumber}
                    onChange={(event) => setContainerNumber(event.target.value.toUpperCase())}
                  />
                  {numberWarning && (
                    <small className="field-warning" role="status">
                      {numberWarning}
                    </small>
                  )}
                </label>
                <Select
                  label="Тип контейнера"
                  value={containerType}
                  options={[
                    { value: '', label: 'Не указан' },
                    ...[
                      ...new Set([...containerTypes, ...(containerType ? [containerType] : [])]),
                    ].map((value) => ({ value, label: value })),
                  ]}
                  onChange={setContainerType}
                />
                {settingsFailure && (
                  <p role="alert" className="error">
                    Типы контейнеров недоступны: {settingsFailure}
                  </p>
                )}
                <Select
                  label="Принадлежность контейнера"
                  value={ownership}
                  options={[
                    { value: '', label: 'Не указана' },
                    { value: 'SOC', label: 'SOC' },
                    { value: 'COC', label: 'COC' },
                  ]}
                  onChange={setOwnership}
                />
              </>
            )}
          </div>
        </fieldset>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <div className="transportation-form-actions">
          <button type="button" disabled={busy} onClick={() => confirmDiscard(dirty, onCancel)}>
            Отменить
          </button>
          <button className="primary" disabled={busy || (Boolean(initial) && !dirty)}>
            {busy ? 'Сохраняем…' : initial ? 'Сохранить' : 'Создать перевозку'}
          </button>
        </div>
      </form>
      {confirmation}
    </>
  );
}
export function NewTransportation(props: Omit<Props, 'initial' | 'onCancel'>) {
  return (
    <section className="creation-page surface">
      <div className="panel-heading">
        <h1>Новая перевозка</h1>
        <a href={`#/shipments/${props.shipmentId}`}>← К поставке</a>
      </div>
      <TransportationDetailsForm
        {...props}
        onCancel={() => {
          location.hash = `/shipments/${props.shipmentId}`;
        }}
      />
    </section>
  );
}
