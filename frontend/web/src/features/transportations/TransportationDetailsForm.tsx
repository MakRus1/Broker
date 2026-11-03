import { saveErrorMessage } from '../../shared/forms/saveError';
import { RecordHeading, RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { containerNumberWarning } from './transportation';
import { useEffect, useState, type FormEvent } from 'react';
import { request } from '../../shared/api/http';
import { transportationTypes } from '../routes/logisticsLabels';
import { type Session } from '../../shared/api/session';
import { type Transportation } from './types';
import { Select } from '../../shared/ui/Select';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';

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
  const { translate, locale } = useLocalization();
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
  const numberWarning = containerNumberWarning(containerNumber, locale);
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
      setFailure(saveErrorMessage(error, translate('forms.couldNotSaveTheTransportation')));
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  return (
    <>
      <form className="transportation-editor" onSubmit={submit} data-dirty={dirty} data-busy={busy}>
        <fieldset disabled={busy}>
          <RecordSection title={translate('transport.details')}>
            <div className="shipment-fields">
              <Select
                label={translate('registryField.type')}
                value={type}
                options={Object.entries(transportationTypes).map(([value, label]) => ({
                  value,
                  label: registryValueLabel('type', value, locale) ?? label,
                }))}
                onChange={(value) => setType(value as Transportation['type'])}
              />
              <label className="transportation-danger">
                <input
                  type="checkbox"
                  checked={hazardous}
                  onChange={(event) => setHazardous(event.target.checked)}
                />
                {translate('registryField.hazardous')}
              </label>
              {type === 'container' && (
                <>
                  <label className="transportation-number-field">
                    {translate('registryField.container_number')}
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
                    label={translate('registryField.container_type')}
                    value={containerType}
                    options={[
                      { value: '', label: translate('routeFilter.pointNotSet') },
                      ...[
                        ...new Set([...containerTypes, ...(containerType ? [containerType] : [])]),
                      ].map((value) => ({ value, label: value })),
                    ]}
                    onChange={setContainerType}
                  />
                  {settingsFailure && (
                    <p role="alert" className="error">
                      {translate('forms.containerTypesUnavailable')}
                      {settingsFailure}
                    </p>
                  )}
                  <Select
                    label={translate('forms.containerOwnership')}
                    value={ownership}
                    options={[
                      { value: '', label: translate('forms.notSpecified') },
                      { value: 'SOC', label: 'SOC' },
                      { value: 'COC', label: 'COC' },
                    ]}
                    onChange={setOwnership}
                  />
                </>
              )}
            </div>
          </RecordSection>
        </fieldset>
        <RecordActions
          error={failure}
          status={
            dirty
              ? translate('shipment.unsaved')
              : initial
                ? translate('shipment.revision', { revision: initial.revision })
                : translate('transport.createHint')
          }
        >
          <button type="button" disabled={busy} onClick={() => confirmDiscard(dirty, onCancel)}>
            {translate('action.cancel')}
          </button>
          <button
            className="primary"
            disabled={busy || (Boolean(initial) && !dirty)}
            title={initial && !dirty ? translate('validation.noChanges') : undefined}
          >
            {busy
              ? translate('shipment.saving')
              : initial
                ? translate('action.save')
                : translate('forms.createTransportation')}
          </button>
        </RecordActions>
      </form>
      {confirmation}
    </>
  );
}
export function NewTransportation(props: Omit<Props, 'initial' | 'onCancel'>) {
  const { translate } = useLocalization();
  return (
    <section className="creation-page surface record-workbench transportation-workbench">
      <RecordHeading
        guide="transportation"
        kind={translate('forms.transportation')}
        title={translate('entity.newTransportation')}
        actions={
          <a className="button" href={`#/shipments/${props.shipmentId}`}>
            {translate('forms.backToShipment')}
          </a>
        }
      />
      <TransportationDetailsForm
        {...props}
        onCancel={() => {
          location.hash = `/shipments/${props.shipmentId}`;
        }}
      />
    </section>
  );
}
