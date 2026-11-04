import { RecordEditorSections } from '../../../shared/ui/record/RecordEditorSections';
import { restoreDraft } from '../../../shared/forms/restoreDraft';
import { draftStorageKey, useDraftRecovery } from '../../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../../shared/ui/record/DraftRecovery';
import { saveErrorMessage } from '../../../shared/forms/saveError';
import {
  RecordHeading,
  RecordSection,
  RecordActions,
} from '../../../shared/ui/record/RecordLayout';
import { useLocalization } from '../../localization/Localization';
import { FinancialHistory } from '../history/FinancialHistory';
import { CardNavigation } from '../../../shared/ui/CardNavigation';
import { DecimalInput } from '../../../shared/ui/DecimalInput';
import { useFieldValidation } from '../../../shared/forms/fieldValidation';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { DateInput } from '../../../shared/dates/DateInput';
import { CustomerSelect, type Counterparty } from '../../counterparties/CustomerSelect';
import { BankAccountSelect } from '../../counterparties/BankAccountSelect';
import { Select } from '../../../shared/ui/Select';
import { currenciesWithCurrent } from '../currencies';
import { useDiscardConfirmation } from '../../../shared/ui/Dialog';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { PaymentRegistry } from './PaymentRegistry';
import { formatFixed, parseFixed } from '../services/serviceCharges';
import {
  paymentBase,
  type PaymentKind,
  type Payment,
  type PaymentDetails,
  type InvoiceBalance,
} from './payments';
const parties: Counterparty[] = [];
const today = () => {
  const d = new Date();
  return CalendarDate.fromISO(
    `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`,
  );
};
const displayDate = (s: string) => CalendarDate.fromISO(s).toDisplayString();
type Draft = Omit<Payment, 'payment_date' | 'amount_minor'> & {
  payment_date: CalendarDate | null;
  amount: string;
};
const draftOf = (p: Payment): Draft => {
  const { amount_minor, payment_date, ...rest } = p;
  return {
    ...rest,
    payment_date: CalendarDate.fromISO(payment_date),
    amount: formatFixed(amount_minor, 2).replace(/\s/g, ''),
  };
};
function empty(kind: PaymentKind): Draft {
  return {
    id: crypto.randomUUID(),
    kind,
    number: '',
    payment_date: today(),
    own_company_id: '',
    own_company_name: '',
    counterparty_id: '',
    counterparty_name: '',
    own_bank_account_id: null,
    counterparty_bank_account_id: null,
    currency: 'RUB',
    amount: '',
    purpose: null,
    note: null,
    archived: false,
    revision: 0,
  };
}
export function PaymentsPage({
  route,
  active,
  session,
  onTitle,
  onSaved,
  onCancel,
}: {
  route: string;
  active: boolean;
  session: Session;
  onTitle: (s: string) => void;
  onSaved: (p: Payment) => void;
  onCancel: () => void;
}) {
  const { translate, locale } = useLocalization();
  const kind: PaymentKind = route.startsWith('/disbursements') ? 'disbursement' : 'receipt';
  const base = paymentBase(kind);
  const receipt = kind === 'receipt';
  const title = receipt ? translate('entity.receipt') : translate('entity.disbursement');
  const id = route.slice(base.length + 1);
  const creating = id === 'new';
  const registry = route === base;
  const [historyOpen, setHistoryOpen] = useState(false);
  useEffect(() => setHistoryOpen(false), [route]);
  const [value, setValue] = useState<PaymentDetails>();
  const [draft, setDraft] = useState<Draft>();
  const [allocations, setAllocations] = useState<Record<string, string>>({});
  const [invoiceMap, setInvoiceMap] = useState<Record<string, InvoiceBalance>>({});
  const [candidates, setCandidates] = useState<InvoiceBalance[]>([]);
  const [after, setAfter] = useState<string | null>();
  const [search, setSearch] = useState('');
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [candidateFailure, setCandidateFailure] = useState('');
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const validation = useFieldValidation();
  const original = useRef('');
  const editing = useRef(false);
  const loadedDocument = useRef('');
  editing.current = !!draft;
  const dirty = !!draft && JSON.stringify({ draft, allocations }) !== original.current;
  const storageKey = draftStorageKey(session, 'payment', `${kind}-${id || 'registry'}`);
  const recovery = useDraftRecovery({
    storageKey,
    revision: value?.payment.revision ?? 0,
    value: { draft, allocations, invoiceMap },
    dirty,
    enabled: !!draft && session.permissions.includes('finance.write_payments') && !loading,
    onRestore(raw) {
      const next = restoreDraft<{
        draft: unknown;
        allocations: Record<string, string>;
        invoiceMap: Record<string, InvoiceBalance>;
      }>(raw);
      const restored = restoreDraft<Draft>(next.draft, ['payment_date']);
      if (
        restored.kind !== kind ||
        typeof restored.id !== 'string' ||
        !next.allocations ||
        Object.values(next.allocations).some((x) => typeof x !== 'string')
      )
        throw new Error('Invalid draft');
      setDraft(restored);
      setAllocations(next.allocations);
      setInvoiceMap(next.invoiceMap);
    },
  });
  function edit(details: PaymentDetails) {
    const next = draftOf(details.payment);
    const selected = Object.fromEntries(
      details.allocations.map((a) => [
        a.invoice_id,
        formatFixed(a.amount_minor, 2).replace(/\s/g, ''),
      ]),
    );
    setDraft(next);
    setAllocations(selected);
    original.current = JSON.stringify({ draft: next, allocations: selected });
  }
  useEffect(() => {
    if (!active || registry || editing.current) return;
    let cancelled = false;
    setFailure('');
    const documentKey = `${session.subject}:${session.organization_id}:${kind}:${id}`;
    if (loadedDocument.current !== documentKey) {
      setDraft(undefined);
      setValue(undefined);
      setInvoiceMap({});
      loadedDocument.current = documentKey;
    }
    // Возврат во вкладку обновляет данные в фоне, не убирая открытую карточку.
    if (creating) {
      const d = empty(kind);
      setDraft(d);
      setAllocations({});
      original.current = JSON.stringify({ draft: d, allocations: {} });
      return;
    }
    setLoading(true);
    request<PaymentDetails>('/api/v1/payments/get', session, { id })
      .then(async (r) => {
        if (cancelled) return;
        if (r.payment.kind !== kind)
          throw new Error(translate('forms.paymentNotFoundInThisSection'));
        setValue(r);
        onTitle(r.payment.number);
        const items = await Promise.all(
          r.allocations.map((a) =>
            request<InvoiceBalance>('/api/v1/payments/invoice-balance', session, {
              id: a.invoice_id,
            }),
          ),
        );
        if (!cancelled) setInvoiceMap(Object.fromEntries(items.map((i) => [i.invoice.id, i])));
      })
      .catch((e) => {
        if (!cancelled) setFailure(e.message);
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
    // Workspace title changes must not reload a document being edited.
  }, [id, creating, registry, active, session, kind]);
  useEffect(() => {
    if (!draft?.own_company_id || !draft.counterparty_id) return;
    const controller = new AbortController();
    const timer = setTimeout(() => {
      setCandidateFailure('');
      setLoading(true);
      request<{ items: InvoiceBalance[]; next_after?: string | null }>(
        '/api/v1/payments/invoice-candidates',
        session,
        {
          kind: draft.kind,
          payment_id: draft.id,
          own_company_id: draft.own_company_id,
          counterparty_id: draft.counterparty_id,
          currency: draft.currency,
          query: search || undefined,
          limit: 40,
        },
        controller.signal,
      )
        .then((r) => {
          if (!controller.signal.aborted) {
            setCandidates(r.items);
            setAfter(r.next_after);
            setInvoiceMap((old) => ({
              ...old,
              ...Object.fromEntries(r.items.map((i) => [i.invoice.id, i])),
            }));
          }
        })
        .catch((e) => {
          if (!controller.signal.aborted) setCandidateFailure(e.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 200);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [
    draft?.kind,
    draft?.own_company_id,
    draft?.counterparty_id,
    draft?.currency,
    search,
    session,
  ]);
  async function more() {
    if (!draft || !after || loading) return;
    setLoading(true);
    try {
      const r = await request<{ items: InvoiceBalance[]; next_after?: string | null }>(
        '/api/v1/payments/invoice-candidates',
        session,
        {
          kind: draft.kind,
          payment_id: draft.id,
          own_company_id: draft.own_company_id,
          counterparty_id: draft.counterparty_id,
          currency: draft.currency,
          query: search || undefined,
          after,
          limit: 40,
        },
      );
      setCandidates((old) => [...old, ...r.items]);
      setInvoiceMap((old) => ({
        ...old,
        ...Object.fromEntries(r.items.map((i) => [i.invoice.id, i])),
      }));
      setAfter(r.next_after);
    } catch (e) {
      setCandidateFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  function cancel() {
    confirmDiscard(dirty, () => {
      recovery.discard();
      setFailure('');
      validation.reset();
      if (creating) onCancel();
      else setDraft(undefined);
    });
  }

  async function changeParties(patch: Partial<Draft>) {
    if (!draft) return;
    if (Object.keys(allocations).length) {
      setFailure(translate('payment.removeAllocationsHint'));
      return;
    }
    setDraft({ ...draft, ...patch });
    setAllocations({});
    setInvoiceMap({});
    setCandidates([]);
    setSearch('');
  }
  async function save(e: FormEvent) {
    e.preventDefault();
    if (!draft || busy || recovery.candidate) return;
    const amount = parseFixed(draft.amount, 2, 9000000000000000);
    if (!amount || !draft.payment_date) {
      setFailure(translate('forms.enterThePaymentAmountAndDate'));
      return;
    }
    const items = Object.entries(allocations).map(([invoice_id, text]) => ({
      invoice_id,
      amount_minor: parseFixed(text, 2, amount),
    }));
    if (items.some((i) => !i.amount_minor)) {
      setFailure(translate('forms.enterAPositiveAmountForEachSelectedInvoice'));
      return;
    }
    if (items.reduce((s, i) => s + (i.amount_minor ?? 0), 0) > amount) {
      setFailure(translate('forms.theAllocatedAmountExceedsThePaymentAmount'));
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const body = {
        id: draft.id,
        kind: draft.kind,
        number: draft.number,
        payment_date: draft.payment_date,
        own_company_id: draft.own_company_id,
        counterparty_id: draft.counterparty_id,
        own_bank_account_id: draft.own_bank_account_id,
        counterparty_bank_account_id: draft.counterparty_bank_account_id,
        currency: draft.currency,
        purpose: draft.purpose,
        note: draft.note,
        archived: draft.archived,
      };
      const result = await request<PaymentDetails>('/api/v1/payments/save', session, {
        ...body,
        expected_revision: draft.revision,
        amount_minor: amount,
        allocations: items,
      });
      setValue(result);
      setDraft(undefined);
      onTitle(result.payment.number);
      recovery.markSaved();
      onSaved(result.payment);
    } catch (e) {
      setFailure(saveErrorMessage(e, translate('errors.request')));
    } finally {
      setBusy(false);
    }
  }
  if (registry)
    return (
      <section className="payment-page registry-page">
        <PaymentRegistry kind={kind} active={active} session={session} />
      </section>
    );
  const write = session.permissions.includes('finance.write_payments');
  const amount = draft
    ? (parseFixed(draft.amount, 2, 9000000000000000) ?? 0)
    : (value?.payment.amount_minor ?? 0);
  const allocated = draft
    ? Object.values(allocations).reduce((s, v) => s + (parseFixed(v, 2, 9000000000000000) ?? 0), 0)
    : (value?.allocated_minor ?? 0);
  const selected = draft
    ? Object.keys(allocations)
    : (value?.allocations.map((a) => a.invoice_id) ?? []);
  return (
    <section className="payment-page surface record-workbench financial-record-workbench">
      {confirmation}
      {write && !loading && <DraftRecovery recovery={recovery} />}
      <RecordHeading
        guide={receipt ? 'receipt' : 'disbursement'}
        kind={title}
        title={
          creating
            ? translate(receipt ? 'forms.newReceipt' : 'forms.newDisbursement')
            : (value?.payment.number ?? title)
        }
        status={
          value ? translate(value.payment.archived ? 'filter.archived' : 'forms.active') : undefined
        }
        metadata={
          value ? translate('forms.revisionValue', { revision: value.payment.revision }) : undefined
        }
        mode={draft ? translate(creating ? 'record.creating' : 'record.editing') : undefined}
        actions={
          !draft && value && write ? (
            <button className="primary" onClick={() => edit(value)}>
              {translate('shipment.edit')}
            </button>
          ) : undefined
        }
      />
      {failure && !draft && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      {value && !draft && (
        <CardNavigation className="record-layout-navigation" tabsClassName="record-layout-tabs">
          <button type="button" aria-pressed={!historyOpen} onClick={() => setHistoryOpen(false)}>
            {translate('shipment.details')}
          </button>
          <button type="button" aria-pressed={historyOpen} onClick={() => setHistoryOpen(true)}>
            {translate('finance.history')}
          </button>
        </CardNavigation>
      )}
      {historyOpen && value && !draft ? (
        <FinancialHistory
          kind="payments"
          id={value.payment.id}
          revision={value.payment.revision}
          session={session}
        />
      ) : draft ? (
        <form
          data-recovery-key={storageKey}
          className="payment-form record-editor-form document-editor"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <RecordEditorSections>
            <fieldset disabled={busy || !!recovery.candidate}>
              <RecordSection title={translate('record.identification')}>
                <div className="record-fields">
                  <label>
                    {translate('forms.paymentNumberRequired')}
                    <input
                      required
                      maxLength={200}
                      value={draft.number}
                      onChange={(e) => setDraft({ ...draft, number: e.target.value })}
                    />
                  </label>
                  <label>
                    {translate('forms.paymentDateRequired')}
                    <DateInput
                      required
                      value={draft.payment_date}
                      onChange={(payment_date) => setDraft({ ...draft, payment_date })}
                    />
                  </label>
                </div>
              </RecordSection>
              <RecordSection title={translate('record.parties')}>
                <div className="record-fields">
                  <CustomerSelect
                    required
                    ownCompany
                    label={translate('finance.ownCompany')}
                    items={parties}
                    disabled={busy}
                    value={draft.own_company_id}
                    session={session}
                    onChange={(own_company_id) =>
                      void changeParties({ own_company_id, own_bank_account_id: null })
                    }
                  />
                  <CustomerSelect
                    required
                    label={receipt ? translate('forms.payer') : translate('shipment.recipient')}
                    items={parties}
                    disabled={busy}
                    value={draft.counterparty_id}
                    session={session}
                    onChange={(counterparty_id) =>
                      void changeParties({ counterparty_id, counterparty_bank_account_id: null })
                    }
                  />
                </div>
              </RecordSection>
              <RecordSection title={translate('record.paymentTerms')}>
                <div className="record-fields">
                  <label>
                    {translate('forms.amountRequired')}
                    <DecimalInput
                      required
                      precision={2}
                      value={draft.amount}
                      onChange={(e) => setDraft({ ...draft, amount: e.target.value })}
                    />
                  </label>
                  <Select
                    label={translate('registryField.currency')}
                    searchable
                    value={draft.currency}
                    options={currenciesWithCurrent(draft.currency, locale)}
                    onChange={(currency) =>
                      void changeParties({
                        currency,
                        own_bank_account_id: null,
                        counterparty_bank_account_id: null,
                      })
                    }
                  />
                  <BankAccountSelect
                    owner={draft.own_company_id}
                    label={translate('finance.ownBankAccount')}
                    value={draft.own_bank_account_id}
                    currency={draft.currency}
                    session={session}
                    onChange={(own_bank_account_id) => setDraft({ ...draft, own_bank_account_id })}
                  />
                  <BankAccountSelect
                    owner={draft.counterparty_id}
                    label={
                      receipt
                        ? translate('forms.payerBankAccount')
                        : translate('forms.recipientBankAccount')
                    }
                    value={draft.counterparty_bank_account_id}
                    currency={draft.currency}
                    session={session}
                    onChange={(counterparty_bank_account_id) =>
                      setDraft({ ...draft, counterparty_bank_account_id })
                    }
                  />
                </div>
              </RecordSection>
              <RecordSection title={translate('record.additional')}>
                <div className="record-fields">
                  <label className="record-wide">
                    {translate('forms.paymentPurpose')}
                    <textarea
                      maxLength={2000}
                      value={draft.purpose ?? ''}
                      onChange={(e) => setDraft({ ...draft, purpose: e.target.value || null })}
                    />
                  </label>
                  <label className="record-wide">
                    {translate('shipment.note')}
                    <textarea
                      maxLength={2000}
                      value={draft.note ?? ''}
                      onChange={(e) => setDraft({ ...draft, note: e.target.value || null })}
                    />
                  </label>
                  <label className="bank-archive-toggle">
                    <input
                      type="checkbox"
                      checked={draft.archived}
                      onChange={(e) => setDraft({ ...draft, archived: e.target.checked })}
                    />
                    {translate('filter.archived')}
                  </label>
                </div>
              </RecordSection>
            </fieldset>
            <section className="payment-allocation">
              <div className="section-heading">
                <h2>{translate('forms.invoiceAllocations')}</h2>
                <span>
                  {translate('forms.unallocatedPrefix')}{' '}
                  <strong>
                    {formatFixed(amount - allocated, 2)} {draft.currency}
                  </strong>
                </span>
              </div>
              {draft.archived && <p className="hint">{translate('payment.archivedHint')}</p>}
              {selected.length > 0 && (
                <table className="payment-allocation-table">
                  <thead>
                    <tr>
                      <th>{translate('registryField.invoice_number')}</th>
                      <th>{translate('forms.paymentAmount')}</th>
                      <th />
                    </tr>
                  </thead>
                  <tbody>
                    {selected.map((id) => (
                      <tr key={id}>
                        <td>
                          {invoiceMap[id]
                            ? translate('finance.invoiceNumberDate', {
                                number: invoiceMap[id].invoice.number,
                                date: displayDate(invoiceMap[id].invoice.invoice_date),
                              })
                            : id}
                        </td>
                        <td>
                          <DecimalInput
                            aria-label={translate('forms.paymentForInvoice', {
                              number: invoiceMap[id]?.invoice.number ?? id,
                            })}
                            required
                            precision={2}
                            value={allocations[id]}
                            onChange={(e) =>
                              setAllocations({ ...allocations, [id]: e.target.value })
                            }
                          />
                        </td>
                        <td>
                          <button
                            type="button"
                            onClick={() =>
                              setAllocations((old) =>
                                Object.fromEntries(
                                  Object.entries(old).filter(([key]) => key !== id),
                                ),
                              )
                            }
                          >
                            {translate('shipment.remove')}
                          </button>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
              {draft.own_company_id && draft.counterparty_id ? (
                <>
                  <label>
                    {translate('forms.searchByInvoiceNumber')}
                    <input
                      value={search}
                      onChange={(e) => setSearch(e.target.value)}
                      placeholder={translate('forms.partOfTheNumber')}
                    />
                  </label>
                  {candidateFailure && (
                    <p className="error" role="alert">
                      {candidateFailure}
                    </p>
                  )}
                  <table className="payment-allocation-table">
                    <thead>
                      <tr>
                        <th>
                          {receipt
                            ? translate('forms.outgoingInvoice')
                            : translate('forms.incomingInvoice')}
                        </th>
                        <th>{translate('registryField.amount')}</th>
                        <th>{translate('registryField.outstanding_minor')}</th>
                        <th />
                      </tr>
                    </thead>
                    <tbody>
                      {candidates
                        .filter((i) => !(i.invoice.id in allocations))
                        .map((i) => (
                          <tr key={i.invoice.id}>
                            <td>
                              № {i.invoice.number}
                              {translate('ui.datePrefix')}
                              {displayDate(i.invoice.invoice_date)}
                            </td>
                            <td>{formatFixed(i.invoice.total_minor, 2)}</td>
                            <td>{formatFixed(i.outstanding_minor, 2)}</td>
                            <td>
                              <button
                                type="button"
                                disabled={i.outstanding_minor <= 0}
                                onClick={() =>
                                  setAllocations({
                                    ...allocations,
                                    [i.invoice.id]: formatFixed(
                                      Math.min(
                                        Math.max(amount - allocated, 0),
                                        i.outstanding_minor,
                                      ),
                                      2,
                                    ).replace(/\s/g, ''),
                                  })
                                }
                              >
                                {translate('forms.add')}
                              </button>
                            </td>
                          </tr>
                        ))}
                    </tbody>
                  </table>
                  {!loading && !candidates.length && (
                    <p className="empty-state">{translate('forms.noMatchingInvoicesFound')}</p>
                  )}
                  <AutoLoadMore
                    enabled={!!after}
                    loading={loading}
                    failed={!!candidateFailure}
                    onLoad={more}
                  />
                </>
              ) : (
                <p className="hint">{translate('forms.paymentPartiesHint')}</p>
              )}
            </section>
          </RecordEditorSections>
          <RecordActions
            error={failure}
            issues={validation.issues}
            onIssueClick={validation.focusIssue}
            status={
              dirty
                ? translate(
                    recovery.persisted && !recovery.failed ? 'draft.local' : 'shipment.unsaved',
                  )
                : translate(creating ? 'record.creating' : 'record.editing')
            }
          >
            <button type="button" disabled={busy} onClick={() => void cancel()}>
              {translate('action.cancel')}
            </button>
            <button className="primary" disabled={busy || !!recovery.candidate}>
              {translate('action.save')}
            </button>
          </RecordActions>
        </form>
      ) : value ? (
        <>
          <dl className="payment-amounts">
            <div>
              <dt>
                {receipt ? translate('forms.receiptAmount') : translate('forms.disbursementAmount')}
              </dt>
              <dd>
                {formatFixed(amount, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
            <div>
              <dt>{translate('forms.allocated')}</dt>
              <dd>
                {formatFixed(allocated, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
            <div>
              <dt>{translate('forms.unallocatedLabel')}</dt>
              <dd>
                {formatFixed(value.unallocated_minor, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
          </dl>
          <dl className="payment-details">
            <div>
              <dt>{translate('registryField.payment_date')}</dt>
              <dd>{displayDate(value.payment.payment_date)}</dd>
            </div>
            <div>
              <dt>{translate('forms.ownLegalEntityLabel')}</dt>
              <dd>{value.payment.own_company_name}</dd>
            </div>
            <div>
              <dt>{receipt ? translate('forms.payer') : translate('shipment.recipient')}</dt>
              <dd>{value.payment.counterparty_name}</dd>
            </div>
            {value.payment.purpose && (
              <div className="record-wide">
                <dt>{translate('forms.paymentPurpose')}</dt>
                <dd>{value.payment.purpose}</dd>
              </div>
            )}
            {value.payment.note && (
              <div className="record-wide">
                <dt>{translate('shipment.note')}</dt>
                <dd>{value.payment.note}</dd>
              </div>
            )}
          </dl>
          <h2>{translate('forms.allocatedInvoices')}</h2>
          <table className="payment-allocation-table">
            <thead>
              <tr>
                <th>{translate('registryField.invoice_number')}</th>
                <th>{translate('forms.paymentAmount')}</th>
              </tr>
            </thead>
            <tbody>
              {value.allocations.map((a) => (
                <tr key={a.invoice_id}>
                  <td>
                    <a
                      className="link"
                      href={`#/${receipt ? 'outgoing' : 'incoming'}-invoices/${a.invoice_id}`}
                    >
                      {invoiceMap[a.invoice_id]
                        ? translate('finance.invoiceNumberDate', {
                            number: invoiceMap[a.invoice_id].invoice.number,
                            date: displayDate(invoiceMap[a.invoice_id].invoice.invoice_date),
                          })
                        : translate('forms.openInvoice')}
                    </a>
                  </td>
                  <td>
                    {formatFixed(a.amount_minor, 2)} {value.payment.currency}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          {!value.allocations.length && (
            <p className="empty-state">{translate('forms.paymentUnallocated')}</p>
          )}
        </>
      ) : loading ? (
        <p>{translate('filter.loading')}</p>
      ) : null}
    </section>
  );
}
