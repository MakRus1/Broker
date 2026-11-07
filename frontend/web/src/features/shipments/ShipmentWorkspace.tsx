import { ShipmentInquiryLink } from '../sales/ShipmentInquiryLink';
import { VisitedPanel } from '../../shared/ui/VisitedPanel';
import { RecordEditorSections } from '../../shared/ui/record/RecordEditorSections';
import { useDraftRecovery, draftStorageKey } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { RecordGuideNotice } from '../../shared/ui/record/RecordGuideNotice';
import { RecordGuideMenu } from '../../shared/ui/record/RecordGuideMenu';
import { GuidedWorkflow } from '../../shared/ui/GuidedWorkflow';
import { preferenceKey } from '../registry/registryPreferences';
import {
  shipmentWorkflow,
  shipmentFieldSteps,
  shipmentContextSteps,
  shipmentStepTab,
  shipmentStepEditing,
} from './shipmentWorkflow';
import { saveErrorMessage } from '../../shared/forms/saveError';
import { RecordHeading, RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import { useLocalization } from '../localization/Localization';
import { ServiceCharges } from '../finance/FinancePanel';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { ShipmentRoute } from './ShipmentRoute';
import type { ShipmentRouteSegment } from '../routes/types';
import { localDate } from '../../shared/dates/calendar';
import { useFieldValidation } from '../../shared/forms/fieldValidation';
import { changeShipmentCustomer } from './shipmentParticipants';
import { useCallback, useEffect, useId, useRef, useState, type FormEvent } from 'react';
import { Commands } from '../../shared/api/commands';
import { request } from '../../shared/api/http';
import { type Session } from '../../shared/api/session';
import { type Shipment } from './types';
import { CustomerSelect, type Counterparty } from '../counterparties/CustomerSelect';
import { Select } from '../../shared/ui/Select';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { ShipmentPanel } from './ShipmentPanel';
import { DirectoryLookup } from '../directories/DirectoryLookup';

export interface ShipmentSettings {
  number_template: string;
  reset_number_yearly: boolean;
  statuses: string[];
  route_types: string[];
  planned_containers: boolean;
  container_types: string[];
  customer_logist?: string | null;
}
type DictionaryValue = { code: string; name: string };
type ShipmentDictionaries = {
  incoterms: DictionaryValue[];
  carriage_terms: DictionaryValue[];
  loading: DictionaryValue[];
  route_types: DictionaryValue[];
};

type Props = {
  active?: boolean;
  route: string;
  session: Session;
  customers: Counterparty[];
  onError: (error: unknown) => void;
  onUpdate: (shipment: Shipment) => void;
  onTitle: (title: string) => void;
  onCancel: () => void;
  onCreated: (shipment: Shipment) => void;
};
export function ShipmentWorkspace(props: Props) {
  const { translate } = useLocalization();
  const creating = /^\/shipments\/new(?:\/|$)/.test(props.route);
  const id = creating ? undefined : props.route.split('/')[2];
  const [shipment, setShipment] = useState<Shipment>();
  const [failure, setFailure] = useState('');
  useEffect(() => {
    if (!id) return;
    const controller = new AbortController();
    request<{ shipment: Shipment }>(
      '/api/v1/shipments/get',
      props.session,
      { shipment_id: id },
      controller.signal,
    )
      .then(({ shipment }) => {
        setShipment(shipment);
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [id, props.session]);
  const titleCallback = useRef(props.onTitle);
  titleCallback.current = props.onTitle;
  useEffect(() => {
    if (shipment) titleCallback.current(shipment.number);
  }, [shipment?.number]);
  const onSaved = useCallback(
    (value: Shipment) => {
      setShipment(value);
      props.onUpdate(value);
    },
    [props.onUpdate],
  );
  if (failure)
    return (
      <p className="error" role="alert">
        {failure}
      </p>
    );
  if (!creating && !shipment) return <p role="status">{translate('shipment.opening')}</p>;
  return <ShipmentCard key={id ?? props.route} {...props} initial={shipment} onSaved={onSaved} />;
}
type CardDraft = {
  number: string;
  shipment_date: string;
  customer_id: string;
  recipient_id: string;
  logist: string;
  cargo_description: string;
  status: string;
  incoterms: string;
  carriage_terms: string;
  loading: string;
  delivery_city: string;
  delivery_country: string;
  delivery_address: string;
  reference: string;
  route_segments: ShipmentRouteSegment[];
};
function draftOf(initial?: Shipment): CardDraft {
  return {
    number: initial?.number ?? '',
    shipment_date: initial?.shipment_date ?? localDate(),
    customer_id: initial?.customer_id ?? '',
    recipient_id: initial?.recipient_id ?? '',
    logist: initial?.logist ?? '',
    cargo_description: initial?.cargo_description ?? '',
    status: initial?.status ?? '',
    incoterms: initial?.incoterms ?? '',
    carriage_terms: initial?.carriage_terms ?? '',
    loading: initial?.loading ?? '',
    delivery_city: initial?.delivery_city ?? '',
    delivery_country: initial?.delivery_country ?? '',
    delivery_address: initial?.delivery_address ?? '',
    reference: initial?.reference ?? '',
    route_segments: initial?.route_segments ?? [],
  };
}
function ShipmentCard({
  initial,
  onSaved,
  ...props
}: Props & { initial?: Shipment; onSaved: (value: Shipment) => void }) {
  const { translate } = useLocalization();
  const formId = useId();
  const validation = useFieldValidation((field) => {
    setTab(field.closest('[data-edit-tab]')?.getAttribute('data-edit-tab') ?? 'details');
  });
  const storageKey = draftStorageKey(props.session, 'shipment', initial?.id ?? 'new');
  const [draft, setDraft] = useState<CardDraft>(() => draftOf(initial));
  const [baseline, setBaseline] = useState(() => JSON.stringify(draftOf(initial)));
  const dirty = JSON.stringify(draft) !== baseline;
  const [tab, setTab] = useState(props.route.endsWith('/route') ? 'route' : 'details');
  useEffect(() => {
    if (props.route.endsWith('/route')) setTab('route');
  }, [props.route]);
  const [editing, setEditing] = useState(!initial);
  const [settings, setSettings] = useState<ShipmentSettings>();
  const [dictionaries, setDictionaries] = useState<ShipmentDictionaries>();
  useEffect(() => {
    const controller = new AbortController();
    request<ShipmentDictionaries>(
      '/api/v1/shipments/dictionaries',
      props.session,
      {},
      controller.signal,
    )
      .then(setDictionaries)
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [props.session]);
  const [settingsFailure, setSettingsFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [suggestions, setSuggestions] = useState<string[]>([]);
  const [cargoFailure, setCargoFailure] = useState('');
  const [cargoOpen, setCargoOpen] = useState(false);
  const [cargoLoading, setCargoLoading] = useState(false);
  const cargoInput = useRef<HTMLTextAreaElement>(null);
  const cargoOptions = useRef<HTMLDivElement>(null);
  const defaultSettingsApplied = useRef(false);
  const automaticLogist = useRef('');
  const manualLogist = useRef(false);
  const automaticStatus = useRef('');
  const manualStatus = useRef(false);
  const [manualNumber, setManualNumber] = useState(Boolean(initial));
  const commands = useRef(new Commands());
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const write = props.session.permissions.includes('shipments.write');
  const [revision, setRevision] = useState(initial?.revision);
  const set = <K extends keyof CardDraft>(key: K, value: CardDraft[K]) =>
    setDraft((old) => ({ ...old, [key]: value }));
  const recovery = useDraftRecovery({
    storageKey,
    revision: revision ?? 0,
    value: draft,
    dirty,
    enabled: editing && write,
    onRestore(raw) {
      const candidate = raw as CardDraft;
      if (
        !candidate ||
        !Array.isArray(candidate.route_segments) ||
        Object.keys(draftOf(initial)).some(
          (key) =>
            key !== 'route_segments' && typeof candidate[key as keyof CardDraft] !== 'string',
        )
      )
        throw new Error('Invalid draft');
      setDraft(candidate);
      setEditing(true);
      setManualNumber(Boolean(candidate.number));
      manualLogist.current = true;
      manualStatus.current = true;
    },
  });
  useEffect(() => {
    const leaving = (event: BeforeUnloadEvent) => {
      if ((editing && dirty) || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    };
    window.addEventListener('beforeunload', leaving);
    return () => window.removeEventListener('beforeunload', leaving);
  }, [editing, dirty, busy]);
  useEffect(() => {
    const controller = new AbortController();
    setSettingsFailure('');
    setSettings(undefined);
    request<ShipmentSettings>(
      '/api/v1/shipments/settings',
      props.session,
      draft.customer_id ? { customer_id: draft.customer_id } : {},
      controller.signal,
    )
      .then((value) => {
        if (controller.signal.aborted) return;
        setSettings(value);
        if (!initial && !defaultSettingsApplied.current) {
          defaultSettingsApplied.current = true;
          setBaseline((previous) =>
            JSON.stringify({
              ...JSON.parse(previous),
              status: value.statuses[0],
              logist: value.customer_logist ?? '',
            }),
          );
        }
        if (!initial) {
          const previousAutomaticLogist = automaticLogist.current;
          const previousAutomaticStatus = automaticStatus.current;
          automaticStatus.current = value.statuses[0];
          automaticLogist.current = value.customer_logist ?? '';
          setDraft((old) => ({
            ...old,
            status:
              !manualStatus.current && (!old.status || old.status === previousAutomaticStatus)
                ? value.statuses[0]
                : old.status,
            logist:
              !manualLogist.current && (!old.logist || old.logist === previousAutomaticLogist)
                ? (value.customer_logist ?? '')
                : old.logist,
          }));
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) setSettingsFailure(error.message);
      });
    return () => controller.abort();
  }, [props.session, draft.customer_id, initial?.id]);
  useEffect(() => {
    if (!cargoOpen || !draft.customer_id) {
      setSuggestions([]);
      setCargoLoading(false);
      setCargoFailure('');
      return;
    }
    const controller = new AbortController();
    setCargoLoading(true);
    setSuggestions([]);
    const timer = setTimeout(() => {
      request<{ items: string[] }>(
        '/api/v1/shipments/cargo-suggestions',
        props.session,
        { customer_id: draft.customer_id, query: draft.cargo_description.slice(0, 200) },
        controller.signal,
      )
        .then((result) => {
          if (!controller.signal.aborted) {
            setSuggestions(result.items);
            setCargoFailure('');
          }
        })
        .catch((error) => {
          if (!controller.signal.aborted) setCargoFailure(error.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setCargoLoading(false);
        });
    }, 250);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [props.session, draft.customer_id, draft.cargo_description, cargoOpen]);
  async function removeSuggestion(value: string) {
    try {
      await request('/api/v1/shipments/cargo-suggestions/remove', props.session, {
        customer_id: draft.customer_id,
        value,
      });
      setSuggestions((old) => old.filter((item) => item !== value));
    } catch (error) {
      setCargoFailure(
        error instanceof Error ? error.message : translate('shipment.deleteSuggestionFailed'),
      );
    }
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (busy || !write || !settings || !dictionaries) return;
    if (!draft.customer_id) {
      setFailure(translate('shipment.chooseCustomer'));
      setTab('details');
      return;
    }
    setBusy(true);
    setFailure('');
    const payload = {
      ...draft,
      number: manualNumber ? draft.number.trim() : undefined,
      recipient_id: draft.recipient_id || null,
      logist: draft.logist.trim() || null,
      cargo_description: draft.cargo_description.trim() || null,
      incoterms: draft.incoterms || null,
      carriage_terms: draft.carriage_terms || null,
      loading: draft.loading || null,
      delivery_city: draft.delivery_city.trim() || null,
      delivery_country: draft.delivery_country || null,
      delivery_address: draft.delivery_address.trim() || null,
    };
    const { route_segments, ...detailsPayload } = payload;
    const original = JSON.parse(baseline) as CardDraft;
    const updatePayload = {
      ...detailsPayload,
      logist: draft.logist === (initial?.logist ?? '') ? undefined : payload.logist,
      route_segments:
        JSON.stringify(route_segments) === JSON.stringify(original.route_segments)
          ? undefined
          : route_segments,
    };
    try {
      const result = initial
        ? await request<{ shipment: Shipment }>('/api/v1/shipments/update', props.session, {
            ...updatePayload,
            shipment_id: initial.id,
            expected_revision: revision,
          })
        : await commands.current.send<{ shipment: Shipment }>(
            '/api/v1/shipments/create',
            props.session,
            payload,
          );
      validation.reset();
      const next = draftOf(result.shipment);
      setDraft(next);
      setBaseline(JSON.stringify(next));
      setRevision(result.shipment.revision);
      recovery.markSaved();
      if (initial) {
        onSaved(result.shipment);
        setEditing(false);
      } else props.onCreated(result.shipment);
    } catch (error) {
      setFailure(saveErrorMessage(error, translate('shipment.saveFailed')));
      props.onError(error);
    } finally {
      setBusy(false);
    }
  }
  const updateFromItems = useCallback(
    (value: Shipment) => {
      if (!dirty) {
        const next = draftOf(value);
        setDraft(next);
        setBaseline(JSON.stringify(next));
        setRevision(value.revision);
      }
      onSaved(value);
    },
    [onSaved, dirty],
  );
  const list = (values: DictionaryValue[]) => [
    { value: '', label: translate('ui.notSelected') },
    ...values.map((value) => ({ value: value.code, label: value.name })),
  ];
  const panels = [
    ['details', translate('shipment.details')],
    ['route', translate('registryField.route')],
    ...(initial && settings?.planned_containers
      ? [['items', translate('shipment.plannedContainers')]]
      : []),
    ...(initial ? [['transportations', translate('nav.transportations')]] : []),
    ...(initial && props.session.permissions.includes('finance.read_service_charges')
      ? [['finance', translate('home.finance')]]
      : []),
  ];
  return (
    <section className="shipment-editor surface record-workbench">
      {write && <DraftRecovery recovery={recovery} />}
      <RecordHeading
        kind={translate('shipment.eyebrow')}
        title={initial?.number ?? translate('entity.newShipment')}
        status={
          initial && !editing ? initial.status : draft.status || translate('shipment.newStatus')
        }
        metadata={
          initial ? (
            <>
              {`${translate('registryField.shipment_date')}: ${initial.shipment_date ? CalendarDate.fromISO(initial.shipment_date).toDisplayString() : '—'} · ${translate('shipment.revision', { revision: revision ?? 0 })}`}
              <ShipmentInquiryLink shipment={initial.id} session={props.session} />
            </>
          ) : (
            translate('shipment.createHint')
          )
        }
        mode={
          editing
            ? translate(initial ? 'shipment.editingShipment' : 'shipment.createMode')
            : undefined
        }
        actions={
          initial && write ? (
            <>
              <span className="record-guide-control">
                <RecordGuideMenu>
                  {['overview', 'context'].map((scope) => (
                    <GuidedWorkflow
                      key={
                        scope === 'overview'
                          ? scope
                          : `${scope}-${tab}-${editing ? 'edit' : 'view'}`
                      }
                      textButton
                      title={
                        scope === 'overview'
                          ? 'guide.overview'
                          : editing
                            ? 'guide.filling'
                            : 'guide.section'
                      }
                      preference={preferenceKey(
                        props.session.subject,
                        'shipments',
                        scope === 'overview'
                          ? 'overview'
                          : `workflow-${tab}-${editing ? 'edit' : 'view'}`,
                      )}
                      steps={
                        scope === 'overview'
                          ? shipmentWorkflow(
                              props.session.permissions.includes('finance.read_service_charges'),
                              settings?.planned_containers === true,
                            ).filter((step) => !shipmentStepEditing(step.id))
                          : shipmentContextSteps(
                              tab,
                              editing,
                              props.session.permissions.includes('finance.read_service_charges'),
                              settings?.planned_containers === true,
                            )
                      }
                      active={props.active !== false}
                      getTarget={(key) => {
                        const root = document.getElementById(formId)?.closest('section');
                        if (!root) return null;
                        if (scope === 'context' && !editing && shipmentFieldSteps.has(key)) {
                          const labels: Record<string, string> = {
                            customer: translate('field.customer'),
                            recipient: translate('shipment.recipient'),
                            logist: translate('shipment.logist'),
                            cargo: translate('shipment.cargoDescription'),
                            terms: 'Incoterms',
                            delivery: translate('shipment.city'),
                          };
                          const field = Array.from(root.querySelectorAll<HTMLElement>('dt')).find(
                            (node) =>
                              node.getClientRects().length > 0 && node.textContent === labels[key],
                          );
                          return key === 'delivery' || key === 'terms'
                            ? (field?.closest<HTMLElement>('.record-layout-section') ?? null)
                            : (field?.parentElement ?? null);
                        }

                        const selector = shipmentFieldSteps.has(key)
                          ? `[data-workflow-field="${key}"]`
                          : key === 'route-edit'
                            ? '.shipment-route-add'
                            : key === 'route-details'
                              ? '.shipment-route-workspace'
                              : key === 'plan-quantities'
                                ? '[data-workflow-panel="items"] .quantities'
                                : key === 'plan-create'
                                  ? '[data-workflow-plan-create]'
                                  : key === 'transport-list'
                                    ? '[data-workflow-panel="transportations"]'
                                    : key === 'finance-summary'
                                      ? '.finance-summary-disclosure'
                                      : key === 'finance-invoice'
                                        ? '.finance-services-table-wrap'
                                        : `[data-workflow-tab="${key}"]`;
                        return (
                          root.querySelector<HTMLElement>(selector) ??
                          root.querySelector<HTMLElement>(
                            `[data-workflow-tab="${shipmentStepTab(key)}"]`,
                          )
                        );
                      }}
                      onOpen={(key, proceed) => {
                        if (scope === 'context') {
                          proceed();
                          return;
                        }
                        const root = document.getElementById(formId)?.closest('section');
                        if (root?.querySelector('form[data-busy="true"]')) return;
                        if (shipmentStepEditing(key)) {
                          // Field steps stay in the same editor and preserve the user's draft.
                          if (!editing && dirty) return;
                          setTab(shipmentStepTab(key));
                          setEditing(true);
                          proceed();
                          return;
                        }
                        confirmDiscard(
                          Boolean(root?.querySelector('form[data-dirty="true"]')),
                          () => {
                            if (editing) {
                              setDraft(JSON.parse(baseline) as CardDraft);
                              setEditing(false);
                              validation.reset();
                              setFailure('');
                            }
                            setTab(shipmentStepTab(key));
                            proceed();
                          },
                        );
                      }}
                    />
                  ))}
                </RecordGuideMenu>
                <RecordGuideNotice
                  context={`${editing ? 'edit' : 'view'}:${tab}`}
                  active={props.active !== false}
                  editing={editing}
                />
              </span>
              {!editing && (
                <button
                  className="primary"
                  onClick={() => {
                    if (tab !== 'details' && tab !== 'route') setTab('details');
                    setEditing(true);
                    setFailure('');
                  }}
                >
                  {translate('shipment.editShipment')}
                </button>
              )}
            </>
          ) : undefined
        }
      />
      {settingsFailure && (
        <p className="error" role="alert">
          {translate('shipment.settingsFailed', { message: settingsFailure })}
        </p>
      )}
      <div className="tabs shipment-tabs record-layout-tabs">
        {panels.map(([key, label]) => (
          <button
            key={key}
            data-workflow-tab={key}
            type="button"
            aria-pressed={tab === key}
            onClick={(event) => {
              if (key === tab || busy) return;
              if (editing && (key === 'details' || key === 'route')) {
                setTab(key);
                return;
              }
              const section = event.currentTarget.closest('section');
              if (section?.querySelector('form[data-busy="true"]')) return;
              const switchTab = () => {
                if (initial && editing) {
                  setDraft(JSON.parse(baseline) as CardDraft);
                  setEditing(false);
                  validation.reset();
                  setFailure('');
                }
                setTab(key);
                // Ссылка на маршрут должна работать и для уже открытой карточки.
                if (initial)
                  location.hash = `/shipments/${initial.id}${key === 'route' ? '/route' : ''}`;
              };
              confirmDiscard(
                dirty || Boolean(section?.querySelector('form[data-dirty="true"]')),
                switchTab,
              );
            }}
          >
            {label}
          </button>
        ))}
      </div>
      {initial && (
        <div hidden={editing || (tab !== 'details' && tab !== 'route')}>
          <ShipmentSummary
            shipment={initial}
            customers={props.customers}
            session={props.session}
            routeOnly={tab === 'route'}
            dictionaries={dictionaries}
          />
        </div>
      )}
      <form
        hidden={!editing}
        id={formId}
        onSubmit={save}
        data-recovery-key={storageKey}
        data-validation-attempted={validation.attempted}
        onInputCapture={validation.onInput}
        onInvalidCapture={validation.onInvalid}
        data-dirty={editing && dirty}
        data-busy={busy}
      >
        <fieldset disabled={busy || !write || !settings || !dictionaries || !!recovery.candidate}>
          <fieldset
            className="shipment-edit-section"
            data-edit-tab="details"
            hidden={tab !== 'details'}
          >
            <RecordEditorSections>
              <RecordSection title={translate('shipment.identification')}>
                <div className="shipment-fields">
                  <label>
                    {translate('registryField.shipmentNumber')}
                    <div className="shipment-number">
                      <input
                        name="number"
                        aria-invalid={Boolean(validation.errors.number) || undefined}
                        aria-describedby={
                          validation.errors.number ? `${formId}-number-error` : undefined
                        }
                        required={manualNumber}
                        readOnly={!manualNumber}
                        maxLength={128}
                        value={draft.number}
                        placeholder={translate('shipment.autoNumber')}
                        onChange={(e) => set('number', e.target.value)}
                      />
                      {!initial && (
                        <button
                          type="button"
                          onClick={() => {
                            setManualNumber((old) => !old);
                            set('number', '');
                          }}
                        >
                          {manualNumber
                            ? translate('shipment.automatic')
                            : translate('shipment.manual')}
                        </button>
                      )}
                    </div>
                    {!manualNumber && (
                      <small>
                        {translate('shipment.numberFormat', {
                          format: settings?.number_template ?? '',
                        })}
                      </small>
                    )}
                    {validation.errors.number && (
                      <small className="field-error" id={`${formId}-number-error`}>
                        {validation.errors.number}
                      </small>
                    )}
                  </label>
                  <label>
                    {translate('shipment.dateRequired')}
                    <IsoDateInput
                      name="shipment_date"
                      aria-invalid={Boolean(validation.errors.shipment_date) || undefined}
                      aria-describedby={
                        validation.errors.shipment_date ? `${formId}-date-error` : undefined
                      }

                      min="1900-01-01"
                      required
                      value={draft.shipment_date}
                      onChange={(dateValue) => set('shipment_date', dateValue)}
                    />
                    {validation.errors.shipment_date && (
                      <small className="field-error" id={`${formId}-date-error`}>
                        {validation.errors.shipment_date}
                      </small>
                    )}
                  </label>
                </div>
              </RecordSection>
              <RecordSection title={translate('shipment.participants')}>
                <div className="shipment-fields">
                  <div data-workflow-field="customer">
                    <CustomerSelect
                      label={translate('shipment.customerRequired')}
                      required
                      error={
                        validation.attempted && !draft.customer_id
                          ? translate('lookup.chooseCustomerDirectory')
                          : ''
                      }
                      session={props.session}
                      items={props.customers}
                      value={draft.customer_id}
                      disabled={busy || !write}
                      onChange={(value) =>
                        setDraft((old) => ({ ...old, ...changeShipmentCustomer(old, value) }))
                      }
                    />
                  </div>
                  <div data-workflow-field="recipient">
                    <CustomerSelect
                      label={translate('shipment.recipient')}
                      session={props.session}
                      items={props.customers}
                      value={draft.recipient_id}
                      disabled={busy || !write}
                      onChange={(value) => set('recipient_id', value)}
                    />
                  </div>
                  <div data-workflow-field="logist">
                    <DirectoryLookup
                      session={props.session}
                      kind="employees"
                      label={translate('shipment.logist')}
                      value={draft.logist}
                      onChange={(value) => {
                        manualLogist.current = true;
                        set('logist', value);
                      }}
                    />
                  </div>
                </div>
              </RecordSection>
              <RecordSection title={translate('shipment.cargoTerms')}>
                <div
                  data-workflow-field="cargo"
                  className="cargo-autocomplete"
                  onBlur={(event) => {
                    if (!event.currentTarget.contains(event.relatedTarget)) setCargoOpen(false);
                  }}
                >
                  <label>
                    {translate('shipment.cargoDescription')}
                    <textarea
                      ref={cargoInput}
                      maxLength={2000}
                      rows={2}
                      value={draft.cargo_description}
                      onFocus={() => setCargoOpen(true)}
                      onChange={(event) => {
                        set('cargo_description', event.target.value);
                        setCargoOpen(true);
                      }}
                      onKeyDown={(event) => {
                        if (event.key === 'Escape') {
                          setCargoOpen(false);
                          event.preventDefault();
                        }
                        if (event.key === 'ArrowDown' && cargoOpen && suggestions.length) {
                          event.preventDefault();
                          cargoOptions.current
                            ?.querySelector<HTMLButtonElement>('[data-cargo-choice]')
                            ?.focus();
                        }
                      }}
                      aria-expanded={cargoOpen}
                      aria-controls={cargoOpen ? `${formId}-cargo-options` : undefined}
                      placeholder={translate('shipment.cargoPlaceholder')}
                    />
                  </label>
                  {cargoOpen && (
                    <div
                      ref={cargoOptions}
                      id={`${formId}-cargo-options`}
                      className="cargo-suggestions"
                      role="region"
                      aria-label={translate('shipment.cargoSuggestions')}
                      onKeyDown={(event) => {
                        if (event.key === 'Escape') {
                          cargoInput.current?.focus();
                          setCargoOpen(false);
                          event.preventDefault();
                        }
                      }}
                    >
                      {cargoLoading ? (
                        <p role="status">{translate('shipment.cargoSearching')}</p>
                      ) : cargoFailure ? (
                        <p role="alert">
                          {translate('shipment.cargoFailed', { message: cargoFailure })}
                        </p>
                      ) : suggestions.length === 0 ? (
                        <p>{translate('shipment.cargoNoMatches')}</p>
                      ) : (
                        <>
                          {suggestions.map((value) => (
                            <div key={value} className="cargo-suggestion">
                              <button
                                type="button"
                                data-cargo-choice
                                onClick={() => {
                                  set('cargo_description', value);
                                  cargoInput.current?.focus();
                                  setCargoOpen(false);
                                }}
                              >
                                {value}
                              </button>
                              {write && (
                                <button
                                  type="button"
                                  className="cargo-remove"
                                  aria-label={translate('shipment.deleteSuggestion', { value })}
                                  onClick={() => void removeSuggestion(value)}
                                >
                                  ×
                                </button>
                              )}
                            </div>
                          ))}
                          {suggestions.length === 20 && <p>{translate('shipment.cargoLimit')}</p>}
                        </>
                      )}
                    </div>
                  )}
                </div>
                <div className="shipment-fields three" data-workflow-field="terms">
                  <Select
                    label="Incoterms"
                    value={draft.incoterms}
                    onChange={(v) => set('incoterms', v)}
                    options={list(dictionaries?.incoterms ?? [])}
                  />
                  <Select
                    label={translate('shipment.carriageTerms')}
                    value={draft.carriage_terms}
                    onChange={(v) => set('carriage_terms', v)}
                    options={list(dictionaries?.carriage_terms ?? [])}
                  />
                  <Select
                    label={translate('shipment.loading')}
                    value={draft.loading}
                    onChange={(v) => set('loading', v)}
                    options={list(dictionaries?.loading ?? [])}
                  />
                </div>
              </RecordSection>
              <RecordSection title={translate('shipment.delivery')}>
                <div className="shipment-fields" data-workflow-field="delivery">
                  <DirectoryLookup
                    session={props.session}
                    kind="cities"
                    label={translate('shipment.deliveryCity')}
                    value={draft.delivery_city}
                    onChange={(value, country) => {
                      setDraft((old) => ({
                        ...old,
                        delivery_city: value,
                        ...(country ? { delivery_country: country } : {}),
                      }));
                    }}
                  />
                  <label>
                    {translate('shipment.countryCode')}
                    <input
                      maxLength={2}
                      pattern="[A-Z]{2}"
                      placeholder="RU"
                      value={draft.delivery_country}
                      onChange={(e) => set('delivery_country', e.target.value.toUpperCase())}
                    />
                  </label>
                  <label className="full">
                    {translate('shipment.deliveryAddress')}
                    <textarea
                      maxLength={1000}
                      rows={2}
                      value={draft.delivery_address}
                      onChange={(e) => set('delivery_address', e.target.value)}
                    />
                  </label>
                </div>
              </RecordSection>
              <RecordSection title={translate('shipment.additional')}>
                <div className="shipment-fields">
                  <Select
                    label={translate('registryField.status')}
                    value={draft.status}
                    onChange={(v) => {
                      manualStatus.current = true;
                      set('status', v);
                    }}
                    options={[...new Set([...(settings?.statuses ?? []), draft.status])]
                      .filter(Boolean)
                      .map((value) => ({ value, label: value }))}
                  />
                  <label>
                    {translate('shipment.note')}
                    <input
                      maxLength={256}
                      value={draft.reference}
                      onChange={(e) => set('reference', e.target.value)}
                    />
                  </label>
                </div>
              </RecordSection>
            </RecordEditorSections>
          </fieldset>
          <fieldset
            className="shipment-edit-section"
            data-edit-tab="route"
            hidden={tab !== 'route'}
          >
            <ShipmentRoute
              segments={draft.route_segments}
              onChange={(values) => set('route_segments', values)}
              allowedTypes={settings?.route_types ?? []}
              types={dictionaries?.route_types ?? []}
              session={props.session}
              customers={props.customers}
            />
          </fieldset>
        </fieldset>
      </form>
      {initial && (
        <VisitedPanel active={tab === 'finance'} className="panel-content">
          <ServiceCharges
            active={props.active !== false && tab === 'finance'}
            scope={{ shipment_id: initial.id, transportation_id: null }}
            session={props.session}
            customers={props.customers}
          />
        </VisitedPanel>
      )}
      {initial && (
        <div
          data-workflow-panel={tab === 'items' ? 'items' : 'transportations'}
          hidden={tab !== 'items' && tab !== 'transportations'}
        >
          <ShipmentPanel
            embedded
            active={props.active !== false && (tab === 'items' || tab === 'transportations')}
            containerTypes={settings?.container_types}
            section={tab === 'items' ? 'items' : 'transportations'}
            initial={initial}
            session={props.session}
            customerName={
              props.customers.find((c) => c.id === draft.customer_id)?.name ??
              translate('field.customer')
            }
            onClose={() => {}}
            onError={props.onError}
            onUpdate={updateFromItems}
          />
        </div>
      )}
      {failure && (!editing || !write) && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      {write && editing && (
        <RecordActions
          error={failure}
          issues={validation.issues}
          onIssueClick={validation.focusIssue}
          status={
            dirty
              ? translate(
                  recovery.persisted && !recovery.failed ? 'draft.local' : 'shipment.unsaved',
                )
              : initial
                ? translate('shipment.revision', { revision: revision ?? 0 })
                : translate('shipment.required')
          }
        >
          {
            <button
              type="button"
              disabled={busy}
              onClick={() =>
                confirmDiscard(dirty, () => {
                  recovery.discard();
                  validation.reset();
                  if (!initial) {
                    props.onCancel();
                    return;
                  }
                  setEditing(false);
                  manualLogist.current = false;
                  manualStatus.current = false;
                  setManualNumber(Boolean(initial));
                  setDraft(JSON.parse(baseline) as CardDraft);
                  setFailure('');
                })
              }
            >
              {translate('shipment.cancel')}
            </button>
          }
          <button
            form={formId}
            type="submit"
            className="primary"
            disabled={busy || !settings || Boolean(initial && !dirty)}
          >
            {busy
              ? translate('shipment.saving')
              : initial
                ? translate('shipment.saveChanges')
                : translate('shipment.create')}
          </button>
        </RecordActions>
      )}
      {confirmation}
    </section>
  );
}

function ShipmentSummary({
  shipment,
  customers,
  session,
  routeOnly,
  dictionaries,
}: {
  shipment: Shipment;
  customers: Counterparty[];
  session: Session;
  routeOnly: boolean;
  dictionaries?: ShipmentDictionaries;
}) {
  const { translate, locale } = useLocalization();
  const [logist, setLogist] = useState('');
  useEffect(() => {
    if (!shipment.logist) {
      setLogist('');
      return;
    }
    const controller = new AbortController();
    request<{ items: { display_name: string }[] }>(
      '/api/organizations/employees/list',
      session,
      { subject: shipment.logist, limit: 1 },
      controller.signal,
    )
      .then(({ items }) => setLogist(items[0]?.display_name ?? translate('ui.employeeUnavailable')))
      .catch(() => {
        if (!controller.signal.aborted) setLogist(translate('ui.employeeUnavailable'));
      });
    return () => controller.abort();
  }, [shipment.logist, session, locale]);
  const company = (id?: string | null) => customers.find((item) => item.id === id)?.name ?? '—';
  const group = (title: string, fields: [string, string | null | undefined, boolean?][]) => (
    <RecordSection title={title}>
      <dl className="record-layout-details">
        {fields.map(([label, value, wide]) => (
          <div key={label} className={wide ? 'record-layout-wide' : undefined}>
            <dt>{label}</dt>
            <dd>{value || '—'}</dd>
          </div>
        ))}
      </dl>
    </RecordSection>
  );
  if (routeOnly)
    return (
      <ShipmentRoute
        segments={shipment.route_segments ?? []}
        allowedTypes={[]}
        types={dictionaries?.route_types ?? []}
        session={session}
        customers={customers}
      />
    );

  return (
    <div className="record-layout-content">
      {group(translate('shipment.participants'), [
        [translate('field.customer'), company(shipment.customer_id)],
        [translate('shipment.recipient'), company(shipment.recipient_id)],
        [translate('shipment.logist'), logist],
      ])}
      {group(translate('shipment.cargoTerms'), [
        [translate('shipment.cargoDescription'), shipment.cargo_description, true],
        ['Incoterms', shipment.incoterms],
        [translate('shipment.carriageTerms'), shipment.carriage_terms],
        [translate('shipment.loading'), shipment.loading],
      ])}
      {group(translate('shipment.delivery'), [
        [translate('shipment.city'), shipment.delivery_city],
        [translate('registryField.country_code'), shipment.delivery_country],
        [translate('shipment.address'), shipment.delivery_address, true],
      ])}
      {group(translate('shipment.additional'), [
        [translate('shipment.note'), shipment.reference, true],
      ])}
    </div>
  );
}
