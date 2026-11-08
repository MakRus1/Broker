import { SalesRfqs } from './rfq/SalesRfqs';
import { SalesProposals } from './SalesProposals';
import { NextActionDate } from './NextActionDate';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { RecordHeading, RecordSection, RecordActions } from '../../shared/ui/record/RecordLayout';
import { RecordEditorSections } from '../../shared/ui/record/RecordEditorSections';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { useFieldValidation } from '../../shared/forms/fieldValidation';
import { useDraftRecovery, draftStorageKey } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { Select } from '../../shared/ui/Select';
import { CustomerSelect } from '../counterparties/CustomerSelect';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { SalesCustomerName } from './SalesCustomerName';
import { DirectoryLookup } from '../directories/DirectoryLookup';
import { useLocalization } from '../localization/Localization';
import { ShipmentRoute } from '../shipments/ShipmentRoute';
import { SalesRegistry } from './SalesRegistry';
import { SalesHistory } from './SalesHistory';
import { SalesContacts } from './SalesContacts';
import { InquiryShipmentLink } from './InquiryShipmentLink';
import {
  saveSalesBody,
  statuses,
  type Inquiry,
  type SalesCard,
  type SalesEntity,
  type SalesResource,
  type SalesSettings,
} from './types';
const emptyParties: never[] = [];
export function SalesPage({
  route,
  session,
  active,
  onTitle,
  onSaved,
  onCancel,
}: {
  route: string;
  session: Session;
  active: boolean;
  onTitle: (title: string) => void;
  onSaved: (route: string, title: string) => void;
  onCancel: () => void;
}) {
  const { translate } = useLocalization();
  const resource: SalesResource = route.startsWith('/inquiries') ? 'inquiries' : 'cards';
  const base = resource === 'cards' ? '/sales' : '/inquiries';
  const clean = route.split('?')[0];
  const id = clean.split('/')[2];
  const creating = id === 'new';
  const editing = creating || clean.endsWith('/edit');
  const customer = creating ? (clean.split('/')[3] ?? '') : '';
  const [settings, setSettings] = useState<SalesSettings>({ revision: 0, stages: [] });
  const [value, setValue] = useState<SalesEntity>();
  const [draft, setDraft] = useState<SalesEntity>();
  const [failure, setFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const [tab, setTab] = useState('main');
  const [version, setVersion] = useState(0);
  const initial = useRef('');
  const titleRef = useRef(onTitle);
  titleRef.current = onTitle;
  const write = session.permissions.includes('sales.write');
  const validation = useFieldValidation();
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  useEffect(() => {
    const controller = new AbortController();
    request<SalesSettings>('/api/v1/sales/settings', session, {}, controller.signal)
      .then((result) => {
        if (!controller.signal.aborted) setSettings(result);
      })
      .catch((e) => {
        if (!controller.signal.aborted) setFailure(e.message);
      });
    return () => controller.abort();
  }, [session]);
  useEffect(() => {
    if (!id) return;
    setFailure('');
    setTab('main');
    validation.reset();
    if (creating) {
      const fields = {
        id: crypto.randomUUID(),
        organization_id: session.organization_id,
        customer_id: customer,
        manager_subject: session.subject,
        revision: 0,
      };
      const next: SalesEntity =
        resource === 'cards'
          ? { ...fields, stage_id: '' }
          : { ...fields, status: 'draft', number: '', route_segments: [], converting: false };
      setValue(undefined);
      setDraft(next);
      initial.current = JSON.stringify(next);
      return;
    }
    const controller = new AbortController();
    request<{ item: SalesEntity }>(
      `/api/v1/sales/${resource}/get`,
      session,
      { id },
      controller.signal,
    )
      .then(({ item }) => {
        if (!controller.signal.aborted) {
          setValue(item);
          setDraft(item);
          initial.current = JSON.stringify(item);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setFailure(e.message);
      });
    return () => controller.abort();
  }, [id, creating, resource, session, version]);
  useEffect(() => {
    if (!active || !value || !('converting' in value) || !value.converting) return;
    const controller = new AbortController();
    let pending = false;
    const timer = setInterval(() => {
      if (pending) return;
      pending = true;
      void request<{ item: Inquiry }>(
        '/api/v1/sales/inquiries/get',
        session,
        { id: value.id },
        controller.signal,
      )
        .then(({ item }) => {
          if (!controller.signal.aborted) {
            setValue(item);
            setDraft(item);
          }
        })
        .catch(() => {})
        .finally(() => {
          pending = false;
        });
    }, 2000);
    return () => {
      clearInterval(timer);
      controller.abort();
    };
  }, [active, value?.id, value && 'converting' in value ? value.converting : false, session]);
  const title = creating
    ? translate(resource === 'cards' ? 'sales.newCard' : 'sales.newInquiry')
    : value && 'number' in value
      ? value.number
      : translate(resource === 'cards' ? 'sales.card' : 'sales.inquiry');
  useEffect(() => {
    if (!id || creating || value?.id === id)
      titleRef.current(
        id ? title : translate(resource === 'cards' ? 'sales.funnel' : 'sales.inquiries'),
      );
  }, [title, id, value?.id]);
  const dirty = editing && !!draft && JSON.stringify(draft) !== initial.current;
  const key = draftStorageKey(session, `sales-${resource}`, creating ? 'new' : (id ?? 'registry'));
  const recovery = useDraftRecovery({
    storageKey: key,
    revision: value?.revision ?? 0,
    value: draft,
    dirty,
    enabled: editing && write && !!draft,
    onRestore(raw) {
      const saved = raw as SalesEntity;
      if (!saved?.id || typeof saved.customer_id !== 'string') throw new Error('Invalid draft');
      setDraft(saved);
    },
  });
  const set = (field: string, content: unknown) =>
    setDraft((old) => (old ? ({ ...old, [field]: content || undefined } as SalesEntity) : old));
  function cancel() {
    confirmDiscard(dirty, () => {
      recovery.discard();
      setDraft(value);
      setFailure('');
      onCancel();
    });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { item } = await request<{ item: SalesEntity }>(
        `/api/v1/sales/${resource}/save`,
        session,
        saveSalesBody(draft),
      );
      setValue(item);
      setDraft(item);
      initial.current = JSON.stringify(item);
      recovery.discard();
      onSaved(`${base}/${item.id}`, 'number' in item ? item.number : title);
    } catch (e) {
      setFailure(e instanceof Error ? e.message : translate('sales.saveFailed'));
    } finally {
      setBusy(false);
    }
  }
  async function createShipment() {
    if (!value || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { item } = await request<{ item: Inquiry }>(
        '/api/v1/sales/inquiries/create-shipment',
        session,
        { id: value.id, expected_revision: value.revision },
      );
      setValue(item);
      setDraft(item);
    } catch (e) {
      setFailure(e instanceof Error ? e.message : translate('sales.saveFailed'));
    } finally {
      setBusy(false);
    }
  }
  if (!session.permissions.includes('sales.read'))
    return (
      <section className="card">
        <p>{translate('sales.forbidden')}</p>
      </section>
    );
  if (!id)
    return (
      <section className="card record-page registry-page registry-workbench">
        <SalesRegistry resource={resource} settings={settings} session={session} active={active} />
      </section>
    );
  const inquiry = draft && 'status' in draft ? draft : undefined;
  const frozen =
    !!inquiry && (inquiry.converting || !!inquiry.shipment_id || inquiry.status === 'agreed');
  return (
    <section className="surface record-workbench sales-workbench">
      {confirmation}
      <RecordHeading
        kind={translate(resource === 'cards' ? 'sales.card' : 'sales.inquiry')}
        title={title}
        status={inquiry && translate(`sales.status.${inquiry.status}`)}
        metadata={
          value && (
            <>
              {translate('sales.revision', { revision: value.revision })}
              {!editing && inquiry?.shipment_id && (
                <>
                  {' · '}
                  <InquiryShipmentLink
                    key={inquiry.shipment_id}
                    id={inquiry.shipment_id}
                    session={session}
                  />
                </>
              )}
            </>
          )
        }
        actions={
          !editing &&
          value && (
            <>
              {write &&
                (!('shipment_id' in value) || !value.shipment_id) &&
                !('converting' in value && value.converting) && (
                  <a className="button primary" href={`#${base}/${id}/edit`}>
                    {translate('sales.edit')}
                  </a>
                )}
              {'status' in value &&
                value.status === 'agreed' &&
                !value.shipment_id &&
                !value.converting &&
                !value.conversion_error &&
                write &&
                session.permissions.includes('shipments.write') && (
                  <button className="primary" disabled={busy} onClick={() => void createShipment()}>
                    {translate('sales.createShipment')}
                  </button>
                )}
            </>
          )
        }
      />
      {!editing && (
        <div className="tabs shipment-tabs record-layout-tabs" role="tablist">
          {[
            'main',
            ...(resource === 'cards' ? ['contacts', 'inquiries'] : ['rfqs', 'proposals']),
            'history',
          ].map((name) => (
            <button
              key={name}
              role="tab"
              aria-selected={tab === name}
              aria-pressed={tab === name}
              onClick={() => setTab(name)}
            >
              {translate(
                name === 'rfqs'
                  ? 'rfq.title'
                  : name === 'proposals'
                    ? 'proposal.title'
                    : name === 'contacts'
                      ? 'sales.contacts'
                      : name === 'main'
                        ? 'sales.main'
                        : name === 'history'
                          ? 'sales.history'
                          : 'sales.inquiries',
              )}
            </button>
          ))}
        </div>
      )}
      {failure && !editing && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {!draft ? (
        <p>{translate('common.loading')}</p>
      ) : tab === 'contacts' && resource === 'cards' && !editing ? (
        <SalesContacts
          card={draft as SalesCard}
          session={session}
          onSaved={(card) => {
            setValue(card);
            setDraft(card);
            initial.current = JSON.stringify(card);
          }}
        />
      ) : tab === 'rfqs' && inquiry && !editing ? (
        <SalesRfqs inquiry={inquiry} session={session} />
      ) : tab === 'proposals' && inquiry && !editing ? (
        <SalesProposals
          inquiry={inquiry}
          session={session}
          onChanged={() => {
            void request<{ item: Inquiry }>('/api/v1/sales/inquiries/get', session, {
              id: inquiry.id,
            })
              .then(({ item }) => {
                setValue(item);
                setDraft(item);
              })
              .catch((e) => setFailure(e.message));
          }}
        />
      ) : tab === 'history' && !editing ? (
        <SalesHistory
          key={`${resource}:${id}`}
          resource={resource}
          id={id}
          session={session}
          settings={settings}
        />
      ) : tab === 'inquiries' && !editing ? (
        <div className="registry-workbench">
          <SalesRegistry
            resource="inquiries"
            customer={draft.customer_id}
            settings={settings}
            session={session}
            active={active}
            title={false}
          />
        </div>
      ) : editing && write ? (
        <form
          className="document-editor counterparty-form"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-recovery-key={key}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <DraftRecovery recovery={recovery} />
          <RecordEditorSections>
            <fieldset className="sales-editor-fields" disabled={busy || !!recovery.candidate}>
              <RecordSection title={translate('sales.parties')}>
                <div className="record-fields record-fields-two">
                  <CustomerSelect
                    items={emptyParties}
                    value={draft.customer_id}
                    onChange={(id) => set('customer_id', id)}
                    session={session}
                    disabled={!creating}
                    required
                    label={translate('sales.customer')}
                  />
                  <DirectoryLookup
                    kind="employees"
                    label={translate('sales.manager')}
                    value={draft.manager_subject}
                    onChange={(subject) => set('manager_subject', subject)}
                    session={session}
                  />
                </div>
              </RecordSection>
              {'stage_id' in draft ? (
                <RecordSection title={translate('sales.work')}>
                  <div className="record-fields record-fields-two">
                    <Select
                      label={translate('sales.stage')}
                      value={draft.stage_id}
                      onChange={(id) => set('stage_id', id)}
                      options={settings.stages
                        .filter((s) => s.enabled || s.id === draft.stage_id)
                        .map((s) => ({ value: s.id, label: s.name }))}
                    />
                    {!settings.stages.length && <p>{translate('sales.configureStages')}</p>}
                    <label>
                      {translate('sales.source')}
                      <input
                        value={draft.source ?? ''}
                        maxLength={200}
                        onChange={(e) => set('source', e.target.value)}
                      />
                    </label>
                    <label>
                      {translate('sales.nextAction')}
                      <input
                        value={draft.next_action ?? ''}
                        maxLength={1000}
                        onChange={(e) => set('next_action', e.target.value)}
                      />
                    </label>
                    <label>
                      {translate('sales.nextActionDate')}
                      <IsoDateInput
                        value={draft.next_action_date ?? ''}
                        onChange={(date) => set('next_action_date', date)}
                      />
                    </label>
                  </div>
                </RecordSection>
              ) : (
                <>
                  <RecordSection title={translate('sales.work')}>
                    <Select
                      label={translate('sales.status')}
                      value={draft.status}
                      onChange={(status) => set('status', status)}
                      options={statuses
                        .filter((status) => !creating || status === 'draft')
                        .map((status) => ({
                          value: status,
                          label: translate(`sales.status.${status}`),
                        }))}
                    />
                    {draft.status === 'agreed' && (
                      <p className="hint">{translate('sales.agreedHint')}</p>
                    )}
                  </RecordSection>
                  <fieldset className="sales-editor-fields" disabled={frozen}>
                    <RecordSection title={translate('sales.cargo')}>
                      <label>
                        <textarea
                          aria-label={translate('sales.cargo')}
                          required={draft.status === 'agreed'}
                          value={draft.cargo_description ?? ''}
                          maxLength={2000}
                          onChange={(e) => set('cargo_description', e.target.value)}
                        />
                      </label>
                      <div className="record-fields record-fields-two">
                        <label>
                          {translate('sales.readyDate')}
                          <IsoDateInput
                            value={draft.ready_date ?? ''}
                            onChange={(v) => set('ready_date', v)}
                          />
                        </label>
                        <label>
                          {translate('sales.deliveryDate')}
                          <IsoDateInput
                            value={draft.desired_delivery_date ?? ''}
                            min={draft.ready_date}
                            onChange={(v) => set('desired_delivery_date', v)}
                          />
                        </label>
                      </div>
                    </RecordSection>
                    <RecordSection title={translate('sales.conditions')}>
                      <div className="record-fields record-fields-two">
                        {(
                          [
                            'incoterms',
                            'carriage_terms',
                            'loading',
                            'delivery_city',
                            'delivery_country',
                            'delivery_address',
                          ] as const
                        ).map((field) => (
                          <label key={field}>
                            {translate(`sales.${field}`)}
                            <input
                              maxLength={
                                field === 'delivery_address'
                                  ? 1000
                                  : field === 'delivery_country'
                                    ? 2
                                    : field === 'delivery_city'
                                      ? 200
                                      : 16
                              }
                              value={draft[field] ?? ''}
                              onChange={(e) =>
                                set(
                                  field,
                                  field === 'delivery_country'
                                    ? e.target.value.toUpperCase()
                                    : e.target.value,
                                )
                              }
                            />
                          </label>
                        ))}
                      </div>
                    </RecordSection>
                    <RecordSection title={translate('sales.route')}>
                      <ShipmentRoute
                        segments={draft.route_segments}
                        onChange={(segments) => set('route_segments', segments)}
                        allowedTypes={[
                          'sea',
                          'air',
                          'international_road',
                          'road',
                          'rail',
                          'international_rail',
                          'pickup',
                        ]}
                        types={[]}
                        session={session}
                        customers={emptyParties}
                      />
                    </RecordSection>
                  </fieldset>
                </>
              )}
              <RecordSection title={translate('sales.note')}>
                <textarea
                  value={draft.note ?? ''}
                  maxLength={10000}
                  onChange={(e) => set('note', e.target.value)}
                />
              </RecordSection>
            </fieldset>
          </RecordEditorSections>
          <RecordActions
            status={
              creating
                ? translate('sales.creating')
                : translate('sales.revision', { revision: draft.revision })
            }
            error={failure}
            issues={validation.issues}
            onIssueClick={validation.focusIssue}
          >
            <button type="button" disabled={busy} onClick={cancel}>
              {translate('action.cancel')}
            </button>
            <button
              className="primary"
              disabled={
                busy ||
                !!recovery.candidate ||
                !draft.customer_id ||
                !draft.manager_subject ||
                ('stage_id' in draft && !draft.stage_id)
              }
            >
              {translate('sales.save')}
            </button>
          </RecordActions>
        </form>
      ) : null}
      {draft && !editing && (
        <div hidden={tab !== 'main'}>
          <RecordSection title={translate('sales.parties')}>
            <dl className="record-layout-details sales-details">
              <div>
                <dt>{translate('sales.customer')}</dt>
                <dd>
                  <SalesCustomerName id={draft.customer_id} session={session} />
                </dd>
              </div>
              <div>
                <dt>{translate('sales.manager')}</dt>
                <dd>
                  <EmployeeName subject={draft.manager_subject} session={session} />
                </dd>
              </div>
            </dl>
          </RecordSection>
          {'stage_id' in draft ? (
            <RecordSection title={translate('sales.work')}>
              <dl className="record-layout-details sales-details">
                <div>
                  <dt>{translate('sales.stage')}</dt>
                  <dd>{settings.stages.find((s) => s.id === draft.stage_id)?.name ?? '—'}</dd>
                </div>
                <div>
                  <dt>{translate('sales.source')}</dt>
                  <dd>{draft.source || '—'}</dd>
                </div>
                <div>
                  <dt>{translate('sales.nextAction')}</dt>
                  <dd>{draft.next_action || '—'}</dd>
                </div>
                <div>
                  <dt>{translate('sales.nextActionDate')}</dt>
                  <dd>
                    <NextActionDate date={draft.next_action_date} />
                  </dd>
                </div>
              </dl>
            </RecordSection>
          ) : (
            <>
              {draft.converting && <p role="status">{translate('sales.converting')}</p>}
              {draft.conversion_error && (
                <p className="error" role="alert">
                  {translate('sales.conversionFailed')} {draft.conversion_error}
                </p>
              )}
              <RecordSection title={translate('sales.cargo')}>
                <p>{draft.cargo_description || '—'}</p>
                <dl className="record-layout-details sales-details">
                  <div>
                    <dt>{translate('sales.readyDate')}</dt>
                    <dd>{draft.ready_date || '—'}</dd>
                  </div>
                  <div>
                    <dt>{translate('sales.deliveryDate')}</dt>
                    <dd>{draft.desired_delivery_date || '—'}</dd>
                  </div>
                </dl>
              </RecordSection>
              <RecordSection title={translate('sales.conditions')}>
                <dl className="record-layout-details sales-details">
                  {(
                    [
                      'incoterms',
                      'carriage_terms',
                      'loading',
                      'delivery_city',
                      'delivery_country',
                      'delivery_address',
                    ] as const
                  ).map((field) => (
                    <div key={field}>
                      <dt>{translate(`sales.${field}`)}</dt>
                      <dd>{draft[field] || '—'}</dd>
                    </div>
                  ))}
                </dl>
              </RecordSection>
              <RecordSection title={translate('sales.route')}>
                <ShipmentRoute
                  segments={draft.route_segments}
                  allowedTypes={[]}
                  types={[]}
                  session={session}
                  customers={emptyParties}
                />
              </RecordSection>
            </>
          )}
          <RecordSection title={translate('sales.note')}>
            <p>{draft.note || '—'}</p>
          </RecordSection>
        </div>
      )}
    </section>
  );
}
