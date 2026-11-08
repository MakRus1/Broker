import { NumberField, PriceFields } from './ProposalFields';
import { RfqQuotePicker } from './rfq/RfqQuotePicker';
import { registryValueLabel } from '../registry/registryLabels';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { RecordHistoryEntry, RecordHistoryDiff } from '../../shared/ui/record/RecordHistory';
import { useFieldValidation } from '../../shared/forms/fieldValidation';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { RecordActions, RecordSection } from '../../shared/ui/record/RecordLayout';
import { RecordEditorSections } from '../../shared/ui/record/RecordEditorSections';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';
import { Select } from '../../shared/ui/Select';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { CustomerSelect } from '../counterparties/CustomerSelect';
import { ServiceChargeLookup } from '../finance/invoices/ServiceChargeLookup';
import { formatFixed, inputFixed } from '../finance/services/serviceCharges';
import { type FinancialLabel, financialLabelUnits } from '../finance/labels/financialLabels';
import { currenciesWithCurrent } from '../finance/currencies';
import { useLocalization } from '../localization/Localization';
import { draftStorageKey, useDraftRecovery } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import type { Inquiry } from './types';
import {
  copyProposal,
  proposalMargin,
  type Proposal,
  type ProposalInput,
  type ProposalPrice,
} from './proposals';
import './salesProposals.css';
const noParties: never[] = [];
export function SalesProposals({
  inquiry,
  session,
  onChanged,
}: {
  inquiry: Inquiry;
  session: Session;
  onChanged: () => void;
}) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<Proposal[]>([]);
  const [selected, setSelected] = useState<Proposal>();
  const [draft, setDraft] = useState<ProposalInput>();
  const [revision, setRevision] = useState(0);
  const [failure, setFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const [version, setVersion] = useState(0);
  const [names, setNames] = useState<Record<string, string>>({});
  const initial = useRef('');
  const [history, setHistory] =
    useState<{ actor: string; created_at: string; snapshot: Proposal }[]>();
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const write =
    session.permissions.includes('sales.write') && !inquiry.shipment_id && !inquiry.converting;
  useEffect(() => {
    const abort = new AbortController();
    request<{ items: Proposal[] }>(
      '/api/v1/sales/proposals/list',
      session,
      { id: inquiry.id },
      abort.signal,
    )
      .then((r) => {
        if (!abort.signal.aborted) setItems(r.items);
      })
      .catch((e) => {
        if (!abort.signal.aborted) setFailure(e.message);
      });
    return () => abort.abort();
  }, [inquiry.id, session, version]);
  const dirty = !!draft && JSON.stringify(draft) !== initial.current;
  const recoveryKey = draftStorageKey(
    session,
    'proposal',
    revision && draft ? draft.id : inquiry.id,
  );
  const recovery = useDraftRecovery({
    storageKey: recoveryKey,
    revision,
    value: draft,
    dirty,
    enabled: !!draft && write,
    onRestore(raw) {
      const v = raw as ProposalInput;
      if (v.inquiry_id !== inquiry.id || !Array.isArray(v.lines)) throw Error('Invalid draft');
      setDraft(v);
    },
  });
  function edit(value: ProposalInput, rev: number) {
    setSelected(undefined);
    setHistory(undefined);
    setDraft(value);
    setRevision(rev);
    initial.current = JSON.stringify(value);
    setFailure('');
  }
  function cancel() {
    confirmDiscard(dirty, () => {
      recovery.discard();
      setDraft(undefined);
      setFailure('');
    });
  }
  function start() {
    if (!inquiry.cargo_description?.trim()) {
      setFailure(translate('proposal.cargoRequired'));
      return;
    }
    const now = new Date();
    const date = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
    edit(
      {
        id: crypto.randomUUID(),
        inquiry_id: inquiry.id,
        title: '',
        calculation_date: date,
        lines: [],
      },
      0,
    );
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const { item } = await request<{ item: Proposal }>('/api/v1/sales/proposals/save', session, {
        ...draft,
        expected_revision: revision,
      });
      recovery.discard();
      setDraft(undefined);
      setSelected(item);
      setItems((previous) => {
        const index = previous.findIndex((value) => value.input.id === item.input.id);
        return index === -1
          ? [...previous, item]
          : previous.map((value, position) => (position === index ? item : value));
      });
      setHistory(undefined);
      setVersion((v) => v + 1);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  async function action(value: Proposal, action: 'offer' | 'accept') {
    setBusy(true);
    setFailure('');
    try {
      const { item } = await request<{ item: Proposal }>(
        '/api/v1/sales/proposals/action',
        session,
        { id: value.input.id, expected_revision: value.revision, action },
      );
      setSelected(item);
      setItems((previous) =>
        previous.map((value) => (value.input.id === item.input.id ? item : value)),
      );
      setHistory(undefined);
      setVersion((v) => v + 1);
      onChanged();
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  const show = selected;
  const currentVersion =
    !selected ||
    items.some(
      (item) => item.input.id === selected.input.id && item.revision === selected.revision,
    );
  const validation = useFieldValidation();
  async function loadHistory(append = false) {
    if (!selected) return;
    setFailure('');
    try {
      const result = await request<{ items: NonNullable<typeof history> }>(
        '/api/v1/sales/proposals/history',
        session,
        {
          id: selected.input.id,
          after_revision: append ? history?.at(-1)?.snapshot.revision : undefined,
        },
      );
      setHistory((old) => (append ? [...(old ?? []), ...result.items] : result.items));
    } catch (e) {
      setFailure((e as Error).message);
    }
  }
  return (
    <div className="sales-proposals">
      {confirmation}
      {!draft && (
        <div className="proposal-toolbar">
          <h2>{translate('proposal.title')}</h2>
          {write && (
            <button className="primary" onClick={start}>
              {translate('proposal.new')}
            </button>
          )}
        </div>
      )}
      {failure && !draft && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {draft ? (
        <form
          className="document-editor"
          onSubmit={save}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
          data-dirty={dirty}
          data-busy={busy}
          data-recovery-key={recoveryKey}
        >
          <DraftRecovery recovery={recovery} />
          <RecordEditorSections>
            <fieldset disabled={busy || !!recovery.candidate}>
              <RecordSection title={translate('proposal.details')}>
                <div className="record-fields record-fields-two">
                  <label>
                    {translate('proposal.name')}
                    <input
                      required
                      maxLength={200}
                      value={draft.title}
                      onChange={(e) => setDraft({ ...draft, title: e.target.value })}
                    />
                  </label>
                  <label>
                    {translate('proposal.date')}
                    <IsoDateInput
                      required
                      value={draft.calculation_date}
                      onChange={(calculation_date) => setDraft({ ...draft, calculation_date })}
                    />
                  </label>
                  <label>
                    {translate('proposal.validUntil')}
                    <IsoDateInput
                      value={draft.valid_until ?? ''}
                      min={draft.calculation_date}
                      onChange={(valid_until) =>
                        setDraft({ ...draft, valid_until: valid_until || undefined })
                      }
                    />
                  </label>
                  <label>
                    {translate('proposal.deliveryDays')}
                    <input
                      type="number"
                      min={1}
                      max={3650}
                      value={draft.delivery_days ?? ''}
                      onChange={(e) =>
                        setDraft({
                          ...draft,
                          delivery_days: e.target.value ? Number(e.target.value) : undefined,
                        })
                      }
                    />
                  </label>
                </div>
                <label>
                  {translate('sales.note')}
                  <textarea
                    maxLength={4000}
                    value={draft.note ?? ''}
                    onChange={(e) => setDraft({ ...draft, note: e.target.value })}
                  />
                </label>
              </RecordSection>
              <RfqQuotePicker
                inquiry={inquiry}
                session={session}
                onAdd={(lines, labels) => {
                  setDraft({ ...draft, lines: [...draft.lines, ...lines] });
                  setNames((previous) => ({ ...previous, ...labels }));
                }}
              />
              {draft.lines.map((line, index) => {
                const update = (next: typeof line) =>
                  setDraft({
                    ...draft,
                    lines: draft.lines.map((v, i) => (i === index ? next : v)),
                  });
                return (
                  <RecordSection
                    key={line.id}
                    title={`${translate('proposal.service')} ${index + 1}`}
                  >
                    {line.quote_source && (
                      <p className="hint">
                        <a href={`#/rfqs/${line.quote_source.rfq_id}`}>{translate('rfq.source')}</a>
                        {' · '}
                        {translate('rfq.snapshotHint')}
                        <button
                          type="button"
                          onClick={() => update({ ...line, quote_source: undefined })}
                        >
                          {translate('rfq.detachSource')}
                        </button>
                      </p>
                    )}
                    <fieldset disabled={!!line.quote_source}>
                      <div className="record-fields record-fields-two">
                        <ServiceChargeLookup<FinancialLabel>
                          session={session}
                          kind="labels"
                          label={translate('proposal.service')}
                          value={line.label_id}
                          name={
                            names[line.label_id] ??
                            items
                              .flatMap((v) => v.lines)
                              .find((v) => v.input.label_id === line.label_id)?.label_name ??
                            ''
                          }
                          disabled={busy}
                          required
                          onChange={(label) => {
                            if (label) {
                              setNames({ ...names, [label.id]: label.name });
                              update({
                                ...line,
                                label_id: label.id,
                                unit: label.unit,
                                sale: {
                                  ...line.sale,
                                  currency: label.currency,
                                  vat_mode: label.vat_mode,
                                  vat_rate_basis_points: label.vat_rate_basis_points ?? undefined,
                                },
                              });
                            } else update({ ...line, label_id: '' });
                          }}
                        />
                        <NumberField
                          label={translate('proposal.quantity')}
                          value={line.quantity_milli}
                          digits={3}
                          max={10000000}
                          onChange={(quantity_milli) => update({ ...line, quantity_milli })}
                        />
                        <Select
                          label={translate('registryField.unit')}
                          value={line.unit}
                          options={(
                            Object.keys(financialLabelUnits) as (keyof typeof financialLabelUnits)[]
                          ).map((unit) => ({
                            value: unit,
                            label: translate(`registryValue.unit.${unit}`),
                          }))}
                          onChange={(unit) => update({ ...line, unit: unit as typeof line.unit })}
                        />
                        <Select
                          label={translate('sales.route')}
                          value={line.route_segment_id ?? ''}
                          options={[
                            { value: '', label: translate('proposal.noSegment') },
                            ...inquiry.route_segments.map((s, i) => ({
                              value: s.id,
                              label: `${i + 1} · ${registryValueLabel('mode', s.type, locale) ?? s.type}`,
                            })),
                          ]}
                          onChange={(route_segment_id) =>
                            update({ ...line, route_segment_id: route_segment_id || undefined })
                          }
                        />
                      </div>
                    </fieldset>
                    <h3>{translate('proposal.sale')}</h3>
                    <PriceFields value={line.sale} onChange={(sale) => update({ ...line, sale })} />
                    {line.purchase ? (
                      <>
                        <h3>{translate('proposal.purchase')}</h3>
                        <fieldset disabled={!!line.quote_source}>
                          <PriceFields
                            value={line.purchase}
                            onChange={(purchase) => update({ ...line, purchase })}
                          />
                          <CustomerSelect
                            disabled={busy}
                            items={noParties}
                            session={session}
                            value={line.supplier_id ?? ''}
                            label={translate('proposal.supplier')}
                            onChange={(supplier_id) =>
                              update({ ...line, supplier_id: supplier_id || undefined })
                            }
                          />
                          <button
                            type="button"
                            onClick={() =>
                              update({ ...line, purchase: undefined, supplier_id: undefined })
                            }
                          >
                            {translate('proposal.removePurchase')}
                          </button>
                        </fieldset>
                      </>
                    ) : (
                      <button
                        type="button"
                        onClick={() =>
                          update({ ...line, purchase: { ...line.sale, price_minor: 0 } })
                        }
                      >
                        {translate('proposal.addPurchase')}
                      </button>
                    )}
                    <label>
                      {translate('sales.note')}
                      <input
                        maxLength={4000}
                        value={line.note ?? ''}
                        onChange={(e) => update({ ...line, note: e.target.value })}
                      />
                    </label>
                    <button
                      type="button"
                      onClick={() =>
                        setDraft({ ...draft, lines: draft.lines.filter((v) => v.id !== line.id) })
                      }
                    >
                      {translate('proposal.removeLine')}
                    </button>
                  </RecordSection>
                );
              })}
              <button
                type="button"
                disabled={draft.lines.length >= 100}
                onClick={() =>
                  setDraft({
                    ...draft,
                    lines: [
                      ...draft.lines,
                      {
                        id: crypto.randomUUID(),
                        label_id: '',
                        unit: 'transportation',
                        quantity_milli: 1000,
                        sale: {
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
                {translate('proposal.addLine')}
              </button>
            </fieldset>
          </RecordEditorSections>
          <RecordActions
            status={translate('proposal.status.draft')}
            error={failure}
            issues={validation.issues}
            onIssueClick={validation.focusIssue}
          >
            <button type="button" disabled={busy} onClick={cancel}>
              {translate('action.cancel')}
            </button>
            <button className="primary" disabled={busy || !draft.lines.length}>
              {translate('proposal.calculate')}
            </button>
          </RecordActions>
        </form>
      ) : (
        <>
          {!items.length && <p className="hint">{translate('proposal.empty')}</p>}
          <div className="proposal-options">
            {items.map((value) => (
              <button
                key={value.input.id}
                aria-pressed={selected?.input.id === value.input.id}
                onClick={() => {
                  setSelected(value);
                  setHistory(undefined);
                }}
              >
                <strong>{value.input.title}</strong>
                <span>
                  {translate(`proposal.status.${value.status}`)} ·{' '}
                  {translate('sales.revision', { revision: value.revision })}
                  {inquiry.accepted_proposal_id === value.input.id
                    ? ' · ' + translate('proposal.selected')
                    : ''}
                </span>
              </button>
            ))}
          </div>
          {show && (
            <>
              <div className="proposal-toolbar">
                <h3>{show.input.title}</h3>
                <div className="proposal-actions">
                  {write && currentVersion && show.status === 'draft' && (
                    <button onClick={() => edit(structuredClone(show.input), show.revision)}>
                      {translate('sales.edit')}
                    </button>
                  )}
                  {write && (
                    <button onClick={() => edit(copyProposal(show), 0)}>
                      {translate('proposal.copy')}
                    </button>
                  )}
                  {write && currentVersion && show.status !== 'accepted' && (
                    <button
                      className="primary"
                      disabled={busy}
                      onClick={() =>
                        void action(show, show.status === 'draft' ? 'offer' : 'accept')
                      }
                    >
                      {translate(show.status === 'draft' ? 'proposal.offer' : 'proposal.accept')}
                    </button>
                  )}
                  <button
                    disabled={busy}
                    onClick={() => {
                      setBusy(true);
                      void import('./proposalPdf')
                        .then(({ proposalPdf }) => proposalPdf(show, locale))
                        .catch((e) => setFailure(e.message))
                        .finally(() => setBusy(false));
                    }}
                  >
                    {translate('proposal.pdf')}
                  </button>
                  <button onClick={() => void loadHistory()}>{translate('sales.history')}</button>
                </div>
              </div>
              <p className="hint">
                {translate('proposal.snapshotHint')} {show.input.calculation_date}
              </p>
              <div className="proposal-table-scroll">
                <table className="proposal-table">
                  <thead>
                    <tr>
                      {(['service', 'quantity', 'purchase', 'sale', 'profit'] as const).map((k) => (
                        <th key={k}>{translate(`proposal.${k}`)}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {show.lines.map((line) => (
                      <tr key={line.input.id}>
                        <td>
                          {line.label_name}
                          {line.supplier_name && <small>{line.supplier_name}</small>}
                        </td>
                        <td>{formatFixed(line.input.quantity_milli, 3)}</td>
                        <td>
                          {line.purchase
                            ? `${formatFixed(line.purchase.net_minor, 2)} ${line.input.purchase?.currency}`
                            : '—'}
                        </td>
                        <td>
                          {formatFixed(line.sale.net_minor, 2)} {line.input.sale.currency}
                        </td>
                        <td>
                          {!line.purchase ||
                          line.input.purchase?.currency === line.input.sale.currency
                            ? `${formatFixed(line.sale.net_minor - (line.purchase?.net_minor ?? 0), 2)} ${line.input.sale.currency}`
                            : '—'}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <h3>{translate('proposal.totals')}</h3>
              <div className="proposal-table-scroll">
                <table className="proposal-table">
                  <thead>
                    <tr>
                      {(['currency', 'purchase', 'sale', 'profit', 'margin'] as const).map((k) => (
                        <th key={k}>
                          {translate(k === 'currency' ? 'registryField.currency' : `proposal.${k}`)}
                        </th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {[...show.totals, show.reporting].map((total, i) => (
                      <tr key={`${total.currency}-${i}`}>
                        <td>
                          {total.currency}
                          {i === show.totals.length ? ' · ' + translate('proposal.reporting') : ''}
                        </td>
                        <td>{formatFixed(total.purchase.net_minor, 2)}</td>
                        <td>{formatFixed(total.sale.net_minor, 2)}</td>
                        <td>{formatFixed(total.sale.net_minor - total.purchase.net_minor, 2)}</td>
                        <td>{proposalMargin(total)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <p className="hint">{translate('proposal.netHint')}</p>
              <p>
                {show.rates
                  .map(
                    (rate) =>
                      `${rate.currency}: ${inputFixed(rate.rate_millionths, 6)} ${show.reporting_currency}`,
                  )
                  .join(' · ')}
              </p>
              {history && (
                <RecordSection title={translate('sales.history')}>
                  {history.map((entry, index) => {
                    const previous = history[index - 1]?.snapshot;
                    const total = entry.snapshot.reporting;
                    return (
                      <RecordHistoryEntry
                        key={entry.snapshot.revision}
                        title={translate(`proposal.status.${entry.snapshot.status}`)}
                        actor={
                          <EmployeeName subject={entry.actor.split('#').at(-1)} session={session} />
                        }
                        occurredAt={entry.created_at}
                        revision={translate('sales.revision', {
                          revision: entry.snapshot.revision,
                        })}
                        initiallyOpen={false}
                      >
                        <RecordHistoryDiff
                          rows={[
                            {
                              key: 'title',
                              label: translate('proposal.name'),
                              before: previous?.input.title ?? '—',
                              after: entry.snapshot.input.title,
                            },
                            {
                              key: 'sale',
                              label: translate('proposal.sale'),
                              before: previous
                                ? formatFixed(previous.reporting.sale.net_minor, 2) +
                                  ' ' +
                                  previous.reporting_currency
                                : '—',
                              after: formatFixed(total.sale.net_minor, 2) + ' ' + total.currency,
                            },
                            {
                              key: 'purchase',
                              label: translate('proposal.purchase'),
                              before: previous
                                ? formatFixed(previous.reporting.purchase.net_minor, 2) +
                                  ' ' +
                                  previous.reporting_currency
                                : '—',
                              after:
                                formatFixed(total.purchase.net_minor, 2) + ' ' + total.currency,
                            },
                          ]}
                        />
                        <button onClick={() => setSelected(entry.snapshot)}>
                          {translate('proposal.openVersion')}
                        </button>
                      </RecordHistoryEntry>
                    );
                  })}
                  {!!history.length && history.length % 100 === 0 && (
                    <button onClick={() => void loadHistory(true)}>
                      {translate('proposal.more')}
                    </button>
                  )}
                </RecordSection>
              )}
            </>
          )}
        </>
      )}
    </div>
  );
}
