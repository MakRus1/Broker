import { RouteEditorFields } from '../routes/RouteEditor';
import type { Counterparty } from '../counterparties/CustomerSelect';
import { routeSegment, routeRequest } from './transportation';
import { RecordPage } from '../../shared/ui/record/RecordPage';
import { draftStorageKey, useDraftRecovery } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { saveErrorMessage } from '../../shared/forms/saveError';
import { RecordHeading, RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import { registryValueLabel } from '../registry/registryLabels';
import { useLocalization } from '../localization/Localization';
import { containerNumberWarning } from './transportation';
import { useEffect, useState, type FormEvent, type ReactNode } from 'react';
import { request } from '../../shared/api/http';
import { transportationTypes } from '../routes/logisticsLabels';
import { type Session } from '../../shared/api/session';
import { type Transportation } from './types';
import type { Shipment } from '../shipments/types';
import { Select } from '../../shared/ui/Select';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';

type Props = {
  initial?: Transportation;
  shipmentRoute?: Transportation['route_segments'];
  customers?: Counterparty[];
  historicalRoute?: ReactNode;
  shipmentId: string;
  session: Session;
  onSaved: (value: Transportation) => void;
  onCancel: () => void;
  onError: (error: unknown) => void;
};
export function TransportationDetailsForm({
  initial: source,
  shipmentRoute,
  customers = [],
  historicalRoute,
  shipmentId,
  session,
  onSaved,
  onCancel,
  onError,
}: Props) {
  const [initial] = useState(source);
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
  const [routeSource] = useState(initial?.route_segments ?? shipmentRoute ?? []);
  const [originalRoute] = useState(() =>
    routeSource.filter((stage) => stage.inherited !== false).map(routeSegment),
  );
  const [routeSegments, setRouteSegments] = useState(originalRoute);
  const routeDirty =
    JSON.stringify(routeSegments.map(routeSegment)) !== JSON.stringify(originalRoute);
  const [id, setId] = useState<string>(() => crypto.randomUUID());
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
    (routeDirty ||
      type !== (initial?.type ?? 'container') ||
      hazardous !== (initial?.hazardous ?? false) ||
      containerType !== (initial?.container_type_code ?? '') ||
      ownership !== (initial?.container_ownership ?? '') ||
      containerNumber !== (initial?.container_number ?? ''));
  const storageKey = draftStorageKey(session, 'transportation', initial?.id ?? `new-${shipmentId}`);
  const recovery = useDraftRecovery({
    storageKey,
    revision: initial?.revision ?? 0,
    value: { id, type, hazardous, containerNumber, containerType, ownership, routeSegments },
    dirty,
    enabled: !saved,
    onRestore(raw) {
      const value = raw as {
        id: string;
        type: Transportation['type'];
        hazardous: boolean;
        containerNumber: string;
        containerType: string;
        ownership: string;
        routeSegments?: typeof routeSegments;
      };
      if (
        !value ||
        typeof value.id !== 'string' ||
        !(value.type in transportationTypes) ||
        typeof value.hazardous !== 'boolean' ||
        (value.routeSegments !== undefined &&
          (!Array.isArray(value.routeSegments) ||
            value.routeSegments.length !== routeSegments.length ||
            value.routeSegments.some(
              (stage, index) =>
                !stage ||
                stage.id !== routeSegments[index].id ||
                stage.mode !== routeSegments[index].mode,
            ))) ||
        [value.containerNumber, value.containerType, value.ownership].some(
          (x) => typeof x !== 'string',
        )
      )
        throw new Error('Invalid draft');
      setId(value.id);
      setType(value.type);
      setHazardous(value.hazardous);
      setContainerNumber(value.containerNumber);
      setContainerType(value.containerType);
      setOwnership(value.ownership);
      if (value.routeSegments) setRouteSegments(value.routeSegments);
    },
  });
  async function submit(event: FormEvent) {
    event.preventDefault();
    if (busy || recovery.candidate) return;
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
          ...(routeDirty
            ? {
                route_segments: routeRequest(
                  initial?.id ?? id,
                  initial?.revision ?? 0,
                  routeSegments,
                  locale,
                ).route_segments,
              }
            : {}),
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
      recovery.markSaved();
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
      <DraftRecovery recovery={recovery} />
      <form
        data-recovery-key={storageKey}
        className="transportation-editor"
        onSubmit={submit}
        data-dirty={dirty}
        data-busy={busy}
      >
        <fieldset disabled={busy || !!recovery.candidate}>
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
          {(routeSegments.length > 0 || !historicalRoute) && (
            <RouteEditorFields
              initial={{ shipment_id: shipmentId, route_segments: routeSource }}
              session={session}
              customers={customers}
              segments={routeSegments}
              onChange={setRouteSegments}
              disabled={busy || !!recovery.candidate}
              onError={setFailure}
            />
          )}
          {historicalRoute}
        </fieldset>
        <RecordActions
          error={failure}
          status={
            dirty
              ? translate(
                  recovery.persisted && !recovery.failed ? 'draft.local' : 'shipment.unsaved',
                )
              : initial
                ? ''
                : translate('transport.createHint')
          }
        >
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              confirmDiscard(dirty, () => {
                recovery.discard();
                onCancel();
              })
            }
          >
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
export function NewTransportation(props: Omit<Props, 'initial' | 'shipmentRoute' | 'onCancel'>) {
  const { translate } = useLocalization();
  const [shipment, setShipment] = useState<Shipment>();
  const [failure, setFailure] = useState('');
  const { session, shipmentId } = props;
  useEffect(() => {
    const controller = new AbortController();
    request<{ shipment: Shipment }>(
      '/api/v1/shipments/get',
      session,
      {
        shipment_id: shipmentId,
      },
      controller.signal,
    )
      .then(({ shipment }) => setShipment(shipment))
      .catch((error: Error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [session, shipmentId]);
  const shipmentRoute = shipment?.route_segments?.map(({ type, ...stage }) => ({
    ...stage,
    mode: type as Transportation['route_segments'][number]['mode'],
  }));
  return (
    <RecordPage className="transportation-workbench">
      <RecordHeading
        guide="transportation"
        kind={translate('forms.transportation')}
        title={translate('entity.newTransportation')}
        mode={translate('record.creating')}
        metadata={
          <a className="record-layout-reference" href={`#/shipments/${shipmentId}`}>
            {translate('registryField.shipment_id')} {shipment?.number || '↗'}
          </a>
        }
      />
      {shipment ? (
        <div className="panel-content transportation-edit-content">
          <TransportationDetailsForm
            {...props}
            shipmentRoute={shipmentRoute}
            onCancel={() => {
              location.hash = `/shipments/${shipmentId}`;
            }}
          />
        </div>
      ) : failure ? (
        <p role="alert" className="error">
          {failure}
        </p>
      ) : (
        <p role="status">{translate('common.loading')}</p>
      )}
    </RecordPage>
  );
}
