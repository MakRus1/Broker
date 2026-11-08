import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useRef, useState, type FormEvent } from 'react';
import type { Session } from '../../../shared/api/session';
import { request } from '../../../shared/api/http';
import { RecordSection, RecordActions } from '../../../shared/ui/record/RecordLayout';
import { RecordEditorSections } from '../../../shared/ui/record/RecordEditorSections';
import { useDiscardConfirmation } from '../../../shared/ui/Dialog';
import { useFieldValidation } from '../../../shared/forms/fieldValidation';
import { draftStorageKey, useDraftRecovery } from '../../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../../shared/ui/record/DraftRecovery';
import { Select } from '../../../shared/ui/Select';
import { IsoDateInput } from '../../../shared/dates/DateInput';
import { DirectoryLookup } from '../../directories/DirectoryLookup';
import { CustomerSelect } from '../../counterparties/CustomerSelect';
import { ServiceChargeLookup } from '../../finance/invoices/ServiceChargeLookup';
import { financialLabelUnits, type FinancialLabel } from '../../finance/labels/financialLabels';
import { registryValueLabel } from '../../registry/registryLabels';
import { useLocalization } from '../../localization/Localization';
import { NumberField, PriceFields } from '../ProposalFields';
import type { Inquiry } from '../types';
import type { Rfq, RfqInput, RfqQuote } from './types';
const empty: never[] = [];
export function RfqEditor({
  value,
  inquiry,
  session,
  onSaved,
  onCancel,
}: {
  value: Rfq;
  inquiry: Inquiry;
  session: Session;
  onSaved: (value: Rfq) => void;
  onCancel: () => void;
}) {
  const { translate, locale } = useLocalization();
  const [draft, setDraft] = useState<RfqInput>(structuredClone(value.input));
  const initial = useRef(JSON.stringify(value.input));
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const validation = useFieldValidation();
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty = initial.current !== JSON.stringify(draft);
  const recoveryKey = draftStorageKey(session, 'rfqs', value.input.id);
  const recovery = useDraftRecovery({
    storageKey: recoveryKey,
    revision: value.revision,
    value: draft,
    dirty,
    enabled: true,
    onRestore(raw) {
      const saved = raw as RfqInput;
      if (saved?.id !== value.input.id || !Array.isArray(saved.quotes))
        throw Error('Invalid draft');
      setDraft(saved);
    },
  });
  const updateQuote = (index: number, next: RfqQuote) =>
    setDraft({ ...draft, quotes: draft.quotes.map((q, i) => (i === index ? next : q)) });
  async function save(event: FormEvent) {
    event.preventDefault();
    if (busy) return;
    setBusy(true);
    setError('');
    try {
      const result = await request<{ item: Rfq }>('/api/v1/sales/rfqs/save', session, {
        ...draft,
        expected_revision: value.revision,
      });
      recovery.discard();
      onSaved(result.item);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form
      className="record-editor sales-proposals"
      onSubmit={save}
      onInvalid={validation.onInvalid}
      onInput={validation.onInput}
      data-dirty={dirty}
      data-busy={busy}
      data-recovery-key={recoveryKey}
      data-validation-attempted={validation.attempted}
    >
      {confirmation}
      <DraftRecovery recovery={recovery} />
      <RecordEditorSections>
        <fieldset disabled={busy || !!recovery.candidate}>
          <RecordSection title={translate('rfq.details')}>
            <div className="record-fields record-fields-two">
              <label>
                {translate('rfq.name')}
                <input
                  required
                  maxLength={200}
                  value={draft.title}
                  onChange={(e) => setDraft({ ...draft, title: e.target.value })}
                />
              </label>
              <Select
                label={translate('rfq.segment')}
                value={draft.route_segment_id}
                options={inquiry.route_segments.map((s, i) => ({
                  value: s.id,
                  label: `${i + 1}. ${registryValueLabel('mode', s.type, locale) ?? s.type} · ${sharedRouteDirection(s, locale)}`,
                }))}
                onChange={(route_segment_id) => setDraft({ ...draft, route_segment_id })}
              />
              <DirectoryLookup
                session={session}
                kind="employees"
                label={translate('rfq.responsible')}
                value={draft.responsible_subject}
                onChange={(responsible_subject) => setDraft({ ...draft, responsible_subject })}
              />
              <label>
                {translate('rfq.answerBy')}
                <IsoDateInput
                  value={draft.answer_by ?? ''}
                  onChange={(answer_by) =>
                    setDraft({ ...draft, answer_by: answer_by || undefined })
                  }
                />
              </label>
              <Select
                label={translate('registryField.status')}
                value={draft.status}
                options={(['in_progress', 'ready', 'cancelled'] as const).map((status) => ({
                  value: status,
                  label: translate(`rfq.status.${status}`),
                }))}
                onChange={(status) => setDraft({ ...draft, status: status as RfqInput['status'] })}
              />
            </div>
          </RecordSection>
          {draft.quotes.map((quote, index) => (
            <RecordSection key={quote.id} title={`${translate('rfq.quote')} ${index + 1}`}>
              <div className="rfq-quote-actions">
                {' '}
                <button
                  type="button"
                  onClick={() =>
                    setDraft({ ...draft, quotes: draft.quotes.filter((_, i) => i !== index) })
                  }
                >
                  {translate('rfq.removeQuote')}
                </button>
              </div>
              <div className="record-fields record-fields-two">
                <CustomerSelect
                  session={session}
                  items={empty}
                  disabled={busy}
                  required
                  label={translate('proposal.supplier')}
                  value={quote.supplier_id}
                  onChange={(supplier_id) =>
                    updateQuote(index, { ...quote, supplier_id, supplier_name: undefined })
                  }
                />
                <label>
                  {translate('rfq.variant')}
                  <input
                    required
                    maxLength={200}
                    value={quote.title}
                    onChange={(e) => updateQuote(index, { ...quote, title: e.target.value })}
                  />
                </label>
                <label>
                  {translate('proposal.validUntil')}
                  <IsoDateInput
                    value={quote.valid_until ?? ''}
                    onChange={(valid_until) =>
                      updateQuote(index, { ...quote, valid_until: valid_until || undefined })
                    }
                  />
                </label>
                <label>
                  {translate('proposal.deliveryDays')}
                  <input
                    type="number"
                    min={1}
                    max={3650}
                    step={1}
                    value={quote.delivery_days ?? ''}
                    onChange={(e) =>
                      updateQuote(index, {
                        ...quote,
                        delivery_days: e.target.value ? Number(e.target.value) : undefined,
                      })
                    }
                  />
                </label>
              </div>
              <div className="rfq-services-heading">
                <h3>{translate('rfq.services')}</h3>
                <p className="hint">{translate('rfq.servicesHint')}</p>
              </div>
              {quote.lines.map((line, i) => {
                const update = (next: typeof line) =>
                  updateQuote(index, {
                    ...quote,
                    lines: quote.lines.map((l, j) => (i === j ? next : l)),
                  });
                return (
                  <div className="rfq-quote-line" key={line.id}>
                    <div className="rfq-line-heading">
                      <strong>
                        {translate('proposal.service')} {i + 1}
                      </strong>
                      <button
                        type="button"
                        disabled={quote.lines.length === 1}
                        onClick={() =>
                          updateQuote(index, {
                            ...quote,
                            lines: quote.lines.filter((_, j) => i !== j),
                          })
                        }
                      >
                        {translate('rfq.removeLine')}
                      </button>{' '}
                    </div>
                    <div className="record-fields rfq-service-fields">
                      <ServiceChargeLookup<FinancialLabel>
                        session={session}
                        kind="labels"
                        disabled={busy}
                        label={translate('proposal.service')}
                        value={line.label_id}
                        name={line.label_name ?? ''}
                        onChange={(v) => {
                          if (v)
                            update({ ...line, label_id: v.id, label_name: v.name, unit: v.unit });
                        }}
                      />
                      <Select
                        label={translate('rfq.unit')}
                        value={line.unit}
                        options={Object.keys(financialLabelUnits).map((unit) => ({
                          value: unit,
                          label: registryValueLabel('unit', unit, locale) ?? unit,
                        }))}
                        onChange={(unit) => update({ ...line, unit: unit as typeof line.unit })}
                      />
                      <NumberField
                        label={translate('proposal.quantity')}
                        digits={3}
                        max={10000000}
                        value={line.quantity_milli}
                        onChange={(quantity_milli) => update({ ...line, quantity_milli })}
                      />
                    </div>
                    <PriceFields
                      value={line.price}
                      onChange={(price) => update({ ...line, price })}
                    />
                  </div>
                );
              })}
              <div className="rfq-actions">
                <button
                  type="button"
                  onClick={() =>
                    updateQuote(index, {
                      ...quote,
                      lines: [
                        ...quote.lines,
                        {
                          id: crypto.randomUUID(),
                          label_id: '',
                          unit: 'transportation',
                          quantity_milli: 1000,
                          price: {
                            price_minor: 0,
                            currency: 'RUB',
                            price_mode: 'net',
                            vat_mode: 'exempt',
                          },
                        },
                      ],
                    })
                  }
                >
                  + {translate('rfq.addService')}
                </button>
              </div>
              <label>
                {translate('rfq.conditions')}
                <textarea
                  maxLength={4000}
                  value={quote.note ?? ''}
                  onChange={(e) => updateQuote(index, { ...quote, note: e.target.value })}
                />
              </label>
            </RecordSection>
          ))}
          <button
            type="button"
            disabled={draft.quotes.length >= 100}
            onClick={() =>
              setDraft({
                ...draft,
                quotes: [
                  ...draft.quotes,
                  {
                    id: crypto.randomUUID(),
                    title: '',
                    supplier_id: '',
                    lines: [
                      {
                        id: crypto.randomUUID(),
                        label_id: '',
                        unit: 'transportation',
                        quantity_milli: 1000,
                        price: {
                          price_minor: 0,
                          currency: 'RUB',
                          price_mode: 'net',
                          vat_mode: 'exempt',
                        },
                      },
                    ],
                  },
                ],
              })
            }
          >
            {translate('rfq.addQuote')}
          </button>
          <RecordSection title={translate('sales.note')}>
            <label>
              <span className="sr-only">{translate('sales.note')}</span>
              <textarea
                maxLength={4000}
                value={draft.note ?? ''}
                onChange={(e) => setDraft({ ...draft, note: e.target.value })}
              />
            </label>
          </RecordSection>
        </fieldset>
      </RecordEditorSections>
      <RecordActions
        status={
          value.revision
            ? translate('sales.revision', { revision: value.revision })
            : translate('sales.creating')
        }
        error={error}
        issues={validation.issues}
        onIssueClick={validation.focusIssue}
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
          type="submit"
          className="primary"
          disabled={
            busy ||
            !draft.route_segment_id ||
            !draft.responsible_subject ||
            draft.quotes.some((q) => !q.supplier_id || q.lines.some((l) => !l.label_id))
          }
        >
          {translate('action.save')}
        </button>
      </RecordActions>
    </form>
  );
}
