import { ServiceCharges } from './FinancePanel';
import { IsoDateInput } from './DateInput';
import { ShipmentRoute } from './ShipmentRoute';
import type { ShipmentRouteSegment } from './api';
import { localDate } from './calendar';
import { useFieldValidation } from './fieldValidation';
import { changeShipmentCustomer } from './shipmentParticipants';
import { useCallback, useEffect, useId, useRef, useState, type FormEvent } from 'react';
import { Commands, request, type Session, type Shipment } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { Select } from './Select';
import { useDiscardConfirmation } from './components';
import { ShipmentPanel } from './ShipmentPanel';
import { DirectoryLookup } from './DirectoryLookup';

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
  onCreated: (shipment: Shipment) => void;
};
export function ShipmentWorkspace(props: Props) {
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
  if (!creating && !shipment) return <p role="status">Открываем поставку…</p>;
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
  const formId = useId();
  const validation = useFieldValidation();
  const storageKey = `broker:shipment-card:${props.session.actor}:${props.route}`;
  const [draft, setDraft] = useState<CardDraft>(() => {
    try {
      const saved = JSON.parse(sessionStorage.getItem(storageKey) ?? 'null');
      if (saved?.revision === (initial?.revision ?? 0) && saved?.value)
        return { ...draftOf(initial), ...saved.value };
    } catch {
      /* Повреждённый черновик не блокирует открытие карточки. */
    }
    return draftOf(initial);
  });
  const [baseline, setBaseline] = useState(() => JSON.stringify(draftOf(initial)));
  const dirty = JSON.stringify(draft) !== baseline;
  const [tab, setTab] = useState(props.route.endsWith('/route') ? 'route' : 'details');
  const [editing, setEditing] = useState(!initial);
  const [editingSection, setEditingSection] = useState<'details' | 'route'>(
    props.route.endsWith('/route') ? 'route' : 'details',
  );
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
  const baselineRevision = useRef(initial?.revision ?? 0);
  const write = props.session.permissions.includes('shipments.write');
  const [revision, setRevision] = useState(initial?.revision);
  const set = <K extends keyof CardDraft>(key: K, value: CardDraft[K]) =>
    setDraft((old) => ({ ...old, [key]: value }));
  useEffect(() => {
    try {
      if (dirty)
        sessionStorage.setItem(
          storageKey,
          JSON.stringify({ revision: baselineRevision.current, value: draft }),
        );
      else sessionStorage.removeItem(storageKey);
    } catch {
      /* Форма работает и при запрете хранилища. */
    }
  }, [storageKey, draft, dirty]);
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
      setCargoFailure(error instanceof Error ? error.message : 'Не удалось удалить подсказку');
    }
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (busy || !write || !settings || !dictionaries) return;
    if ((!initial || editingSection === 'details') && !draft.customer_id) {
      setFailure('Выберите клиента');
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
    const updatePayload =
      editingSection === 'route'
        ? { route_segments }
        : {
            ...detailsPayload,
            logist: draft.logist === (initial?.logist ?? '') ? undefined : payload.logist,
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
      baselineRevision.current = result.shipment.revision;
      try {
        sessionStorage.removeItem(storageKey);
      } catch {
        /* Успешное сохранение на сервере не зависит от браузерного хранилища. */
      }
      if (initial) {
        onSaved(result.shipment);
        setEditing(false);
      } else props.onCreated(result.shipment);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить поставку');
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
        baselineRevision.current = value.revision;
      }
      onSaved(value);
    },
    [onSaved, dirty],
  );
  const list = (values: DictionaryValue[]) => [
    { value: '', label: 'Не выбрано' },
    ...values.map((value) => ({ value: value.code, label: value.name })),
  ];
  const panels = [
    ['details', 'Основное'],
    ['route', 'Маршрут'],
    ...(initial && settings?.planned_containers ? [['items', 'Плановые контейнеры']] : []),
    ...(initial ? [['transportations', 'Перевозки']] : []),
    ...(initial && props.session.permissions.includes('finance.read_service_charges')
      ? [['finance', 'Финансы']]
      : []),
  ];
  return (
    <section className="shipment-editor surface">
      <div className="page-heading">
        <div>
          <p className="eyebrow">ПОСТАВКА</p>
          <h1>{initial?.number ?? 'Новая поставка'}</h1>
        </div>
        <div className="shipment-card-actions">
          <span className="badge">
            {initial && !editing ? initial.status : draft.status || 'Новая'}
          </span>
          {initial && !editing && write && (tab === 'details' || tab === 'route') && (
            <button
              className="primary"
              onClick={() => {
                setEditingSection(tab);
                setEditing(true);
                setFailure('');
              }}
            >
              {tab === 'route' ? 'Редактировать маршрут' : 'Редактировать'}
            </button>
          )}
        </div>
      </div>
      {settingsFailure && (
        <p className="error" role="alert">
          Не удалось получить настройки: {settingsFailure}
        </p>
      )}
      <div className="tabs shipment-tabs">
        {panels.map(([key, label]) => (
          <button
            key={key}
            type="button"
            aria-pressed={tab === key}
            onClick={(event) => {
              if (key === tab) return;
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
      {initial && !editing && (tab === 'details' || tab === 'route') && (
        <ShipmentSummary
          shipment={initial}
          customers={props.customers}
          session={props.session}
          routeOnly={tab === 'route'}
          dictionaries={dictionaries}
        />
      )}
      <form
        hidden={!editing}
        id={formId}
        onSubmit={save}
        data-validation-attempted={validation.attempted}
        onInputCapture={validation.onInput}
        onInvalidCapture={(event) => {
          setTab(initial ? editingSection : 'details');
          validation.onInvalid(event);
        }}
        data-draft-key={storageKey}
        data-dirty={editing && dirty}
        data-busy={busy}
      >
        <fieldset disabled={busy || !write || !settings || !dictionaries}>
          <fieldset
            className="shipment-edit-section"
            disabled={Boolean(initial && editingSection !== 'details')}
            hidden={tab !== 'details'}
          >
            <div className="shipment-fields">
              <label>
                Номер поставки
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
                    placeholder="Присвоится при создании"
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
                      {manualNumber ? 'Автоматически' : 'Вручную'}
                    </button>
                  )}
                </div>
                {!manualNumber && <small>Формат: {settings?.number_template}</small>}
                {validation.errors.number && (
                  <small className="field-error" id={`${formId}-number-error`}>
                    {validation.errors.number}
                  </small>
                )}
              </label>
              <label>
                Дата поставки · обязательно
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
            <section className="shipment-field-section">
              <h2>Участники</h2>
              <div className="shipment-fields">
                <CustomerSelect
                  label="Клиент · обязательно"
                  required
                  error={
                    validation.attempted && !draft.customer_id
                      ? 'Выберите клиента из справочника'
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
                <CustomerSelect
                  label="Получатель"
                  session={props.session}
                  items={props.customers}
                  value={draft.recipient_id}
                  disabled={busy || !write}
                  onChange={(value) => set('recipient_id', value)}
                />
                <DirectoryLookup
                  session={props.session}
                  kind="employees"
                  label="Логист"
                  value={draft.logist}
                  onChange={(value) => {
                    manualLogist.current = true;
                    set('logist', value);
                  }}
                />
              </div>
            </section>
            <section className="shipment-field-section">
              <h2>Груз и условия</h2>
              <div
                className="cargo-autocomplete"
                onBlur={(event) => {
                  if (!event.currentTarget.contains(event.relatedTarget)) setCargoOpen(false);
                }}
              >
                <label>
                  Описание груза
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
                    placeholder="Введите описание или выберите подсказку"
                  />
                </label>
                {cargoOpen && (
                  <div
                    ref={cargoOptions}
                    id={`${formId}-cargo-options`}
                    className="cargo-suggestions"
                    role="region"
                    aria-label="Подсказки описания груза"
                    onKeyDown={(event) => {
                      if (event.key === 'Escape') {
                        cargoInput.current?.focus();
                        setCargoOpen(false);
                        event.preventDefault();
                      }
                    }}
                  >
                    {cargoLoading ? (
                      <p role="status">Ищем описания…</p>
                    ) : cargoFailure ? (
                      <p role="alert">Подсказки недоступны: {cargoFailure}</p>
                    ) : suggestions.length === 0 ? (
                      <p>Совпадений нет. Можно сохранить своё описание.</p>
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
                                aria-label={`Удалить подсказку ${value}`}
                                onClick={() => void removeSuggestion(value)}
                              >
                                ×
                              </button>
                            )}
                          </div>
                        ))}
                        {suggestions.length === 20 && (
                          <p>Показаны первые 20 совпадений. Уточните описание.</p>
                        )}
                      </>
                    )}
                  </div>
                )}
              </div>
              <div className="shipment-fields three">
                <Select
                  label="Incoterms"
                  value={draft.incoterms}
                  onChange={(v) => set('incoterms', v)}
                  options={list(dictionaries?.incoterms ?? [])}
                />
                <Select
                  label="Условия перевозки"
                  value={draft.carriage_terms}
                  onChange={(v) => set('carriage_terms', v)}
                  options={list(dictionaries?.carriage_terms ?? [])}
                />
                <Select
                  label="Загрузка"
                  value={draft.loading}
                  onChange={(v) => set('loading', v)}
                  options={list(dictionaries?.loading ?? [])}
                />
              </div>
            </section>
            <section className="shipment-field-section">
              <h2>Доставка</h2>
              <div className="shipment-fields">
                <DirectoryLookup
                  session={props.session}
                  kind="cities"
                  label="Город доставки"
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
                  Код страны
                  <input
                    maxLength={2}
                    pattern="[A-Z]{2}"
                    placeholder="RU"
                    value={draft.delivery_country}
                    onChange={(e) => set('delivery_country', e.target.value.toUpperCase())}
                  />
                </label>
                <label className="full">
                  Адрес доставки
                  <textarea
                    maxLength={1000}
                    rows={2}
                    value={draft.delivery_address}
                    onChange={(e) => set('delivery_address', e.target.value)}
                  />
                </label>
              </div>
            </section>
            <section className="shipment-field-section">
              <div className="shipment-fields">
                <Select
                  label="Статус"
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
                  Примечание
                  <input
                    maxLength={256}
                    value={draft.reference}
                    onChange={(e) => set('reference', e.target.value)}
                  />
                </label>
              </div>
            </section>
          </fieldset>
          <fieldset
            className="shipment-edit-section"
            disabled={Boolean(initial && editingSection !== 'route')}
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
      {initial && tab === 'finance' && (
        <div className="panel-content">
          <ServiceCharges
            scope={{ shipment_id: initial.id, transportation_id: null }}
            session={props.session}
            customers={props.customers}
          />
        </div>
      )}
      {initial && (
        <div hidden={tab !== 'items' && tab !== 'transportations'}>
          <ShipmentPanel
            embedded
            active={props.active}
            containerTypes={settings?.container_types}
            section={tab === 'items' ? 'items' : 'transportations'}
            initial={initial}
            session={props.session}
            customerName={props.customers.find((c) => c.id === draft.customer_id)?.name ?? 'Клиент'}
            onClose={() => {}}
            onError={props.onError}
            onUpdate={updateFromItems}
          />
        </div>
      )}
      {failure && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      {write && editing && (
        <div className="shipment-save">
          <span>
            {dirty
              ? 'Есть несохранённые изменения'
              : initial
                ? `Редакция ${revision}`
                : 'Обязательно: дата поставки и клиент'}
          </span>
          <div className="shipment-save-actions">
            {(dirty || initial) && (
              <button
                type="button"
                disabled={busy}
                onClick={() =>
                  confirmDiscard(dirty, () => {
                    validation.reset();
                    if (initial) setEditing(false);
                    manualLogist.current = false;
                    manualStatus.current = false;
                    setManualNumber(Boolean(initial));
                    setDraft(JSON.parse(baseline) as CardDraft);
                    setFailure('');
                  })
                }
              >
                Отмена
              </button>
            )}
            <button
              form={formId}
              type="submit"
              className="primary"
              disabled={busy || !settings || Boolean(initial && !dirty)}
            >
              {busy
                ? 'Сохраняем…'
                : initial
                  ? editingSection === 'route'
                    ? 'Сохранить маршрут'
                    : 'Сохранить изменения'
                  : 'Создать поставку'}
            </button>
          </div>
        </div>
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
      .then(({ items }) => setLogist(items[0]?.display_name ?? 'Сотрудник недоступен'))
      .catch(() => {
        if (!controller.signal.aborted) setLogist('Сотрудник недоступен');
      });
    return () => controller.abort();
  }, [shipment.logist, session]);
  const company = (id?: string | null) => customers.find((item) => item.id === id)?.name ?? '—';
  const group = (title: string, fields: [string, string | null | undefined][]) => (
    <section className="shipment-summary-section">
      <h2>{title}</h2>
      <dl className="shipment-summary-grid">
        {fields.map(([label, value]) => (
          <div key={label}>
            <dt>{label}</dt>
            <dd>{value || '—'}</dd>
          </div>
        ))}
      </dl>
    </section>
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
    <>
      {group('Участники', [
        ['Клиент', company(shipment.customer_id)],
        ['Получатель', company(shipment.recipient_id)],
        ['Логист', logist],
      ])}
      {group('Груз и условия', [
        ['Описание груза', shipment.cargo_description],
        ['Incoterms', shipment.incoterms],
        ['Условия перевозки', shipment.carriage_terms],
        ['Загрузка', shipment.loading],
      ])}
      {group('Доставка', [
        ['Город', shipment.delivery_city],
        ['Страна', shipment.delivery_country],
        ['Адрес', shipment.delivery_address],
      ])}
      {group('Поставка', [
        ['Дата поставки', shipment.shipment_date?.split('-').reverse().join('.')],
        ['Примечание', shipment.reference],
      ])}
    </>
  );
}
