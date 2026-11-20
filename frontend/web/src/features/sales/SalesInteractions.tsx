import { RecordHistoryEntry, RecordHistoryDiff } from '../../shared/ui/record/RecordHistory';
import { useEffect, useState, type FormEvent } from 'react';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { IsoDateInput } from '../../shared/dates/DateInput';
import { Select } from '../../shared/ui/Select';
import { EmployeeName } from '../../shared/profile/EmployeeName';
import { useLocalization } from '../localization/Localization';
import type { SalesCard } from './types';
import './salesInteractions.css';
import { useDraftRecovery, draftStorageKey } from '../../shared/forms/useDraftRecovery';
import { DraftRecovery } from '../../shared/ui/record/DraftRecovery';
import { useDiscardConfirmation } from '../../shared/ui/Dialog';

type InteractionKind = 'call' | 'email' | 'meeting' | 'other';
interface Interaction {
  id: string;
  revision: number;
  card_id: string;
  kind: InteractionKind;
  contact_date: string;
  result: string;
  author_subject: string;
  author_issuer: string;
  created_at: string;
  next_action?: string;
  next_action_date?: string;
}
const kinds: InteractionKind[] = ['call', 'email', 'meeting', 'other'];
export function SalesInteractions({
  card,
  session,
  onSaved,
}: {
  card: SalesCard;
  session: Session;
  onSaved: (card: SalesCard) => void;
}) {
  const { translate } = useLocalization();
  const [items, setItems] = useState<Interaction[]>([]);
  const [before, setBefore] = useState<string>();
  const [loaded, setLoaded] = useState(false);
  const [failure, setFailure] = useState('');
  const [busy, setBusy] = useState(false);
  const [form, setForm] = useState<{
    id: string;
    revision?: number;
    kind: InteractionKind;
    contact_date: string;
    result: string;
    next_action: string;
    next_action_date: string;
  }>();
  const [history, setHistory] = useState<
    { revision: number; actor: string; created_at: string; snapshot: Interaction }[]
  >([]);
  const [historyError, setHistoryError] = useState('');
  const original = form?.revision ? items.find((item) => item.id === form.id) : undefined;
  const dirty =
    !!form &&
    (form.revision
      ? (['kind', 'contact_date', 'result', 'next_action', 'next_action_date'] as const).some(
          (field) => form[field] !== (original?.[field] ?? ''),
        )
      : !!form.result || !!form.next_action || !!form.next_action_date);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const recovery = useDraftRecovery({
    storageKey: draftStorageKey(session, 'sales-contact', card.id),
    revision: card.revision,
    value: form ?? {},
    dirty,
    enabled: session.permissions.includes('sales.write'),
    onRestore(raw) {
      const restored = raw as NonNullable<typeof form>;
      if (
        !restored ||
        !restored.id ||
        !kinds.includes(restored.kind) ||
        typeof restored.result !== 'string'
      )
        throw new Error('Invalid contact draft');
      setForm(restored);
    },
  });
  useEffect(() => {
    const controller = new AbortController();
    request<{ items: Interaction[]; next_before?: string }>(
      '/api/v1/sales/contacts/list',
      session,
      { card_id: card.id },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setBefore(result.next_before);
          setLoaded(true);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [card.id, session]);
  async function save(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!form || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ item: Interaction; card: SalesCard }>(
        form.revision ? '/api/v1/sales/contacts/update' : '/api/v1/sales/contacts/add',
        session,
        {
          id: form.id,
          card_id: card.id,
          expected_revision: form.revision ?? card.revision,
          kind: form.kind,
          contact_date: form.contact_date,
          result: form.result.trim(),
          ...(form.next_action.trim() || form.next_action_date
            ? {
                next_action: form.next_action.trim(),
                next_action_date: form.next_action_date,
              }
            : {}),
        },
      );
      setItems((current) =>
        [result.item, ...current.filter((item) => item.id !== result.item.id)].sort(
          (a, b) =>
            b.contact_date.localeCompare(a.contact_date) ||
            b.created_at.localeCompare(a.created_at),
        ),
      );
      recovery.markSaved();
      if (!form.revision) onSaved(result.card);
      else {
        const current = await request<{ item: SalesCard }>('/api/v1/sales/cards/get', session, {
          id: card.id,
        });
        onSaved(current.item);
      }
      setForm(undefined);
    } catch (error) {
      setFailure((error as Error).message);
    } finally {
      setBusy(false);
    }
  }
  async function edit(item: Interaction) {
    if (!session.permissions.includes('sales.write') || form || busy) return;
    setForm({
      id: item.id,
      revision: item.revision,
      kind: item.kind,
      contact_date: item.contact_date,
      result: item.result,
      next_action: item.next_action ?? '',
      next_action_date: item.next_action_date ?? '',
    });
    setFailure('');
    setHistory([]);
    setHistoryError('');
    try {
      const result = await request<{ items: typeof history }>(
        '/api/v1/sales/contacts/history',
        session,
        { id: item.id },
      );
      setHistory(result.items);
    } catch (error) {
      setHistoryError((error as Error).message);
    }
  }
  async function loadMore() {
    if (!before || busy) return;
    setBusy(true);
    setFailure('');
    try {
      const result = await request<{ items: Interaction[]; next_before?: string }>(
        '/api/v1/sales/contacts/list',
        session,
        { card_id: card.id, before },
      );
      setItems((current) => [
        ...current,
        ...result.items.filter((item) => !current.some((saved) => saved.id === item.id)),
      ]);
      setBefore(result.next_before);
    } catch (error) {
      setFailure((error as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <section className="sales-interactions">
      <div className="sales-interactions-heading">
        <h2>{translate('sales.interactions')}</h2>
        {session.permissions.includes('sales.write') && !form && (
          <button
            className="primary"
            onClick={() => {
              const today = new Date();
              setForm({
                id: crypto.randomUUID(),
                kind: 'call',
                contact_date: `${today.getFullYear()}-${String(today.getMonth() + 1).padStart(2, '0')}-${String(today.getDate()).padStart(2, '0')}`,
                result: '',
                next_action: '',
                next_action_date: '',
              });
              setFailure('');
            }}
          >
            {translate('sales.interaction.add')}
          </button>
        )}
      </div>
      {confirmation}
      <DraftRecovery recovery={recovery} />
      {form && (
        <form className="document-editor" onSubmit={save}>
          <h3>{translate(form.revision ? 'sales.interaction.edit' : 'sales.interaction.add')}</h3>
          <div className="sales-interaction-fields">
            <div>
              <Select
                label={translate('sales.interaction.kind')}
                value={form.kind}
                onChange={(kind) => setForm({ ...form, kind: kind as InteractionKind })}
                options={kinds.map((kind) => ({
                  value: kind,
                  label: translate(`sales.interaction.${kind}`),
                }))}
              />
            </div>
            <label>
              {translate('sales.interaction.date')}
              <IsoDateInput
                required
                value={form.contact_date}
                onChange={(contact_date) => setForm({ ...form, contact_date })}
              />
            </label>
            <label className="sales-interaction-wide">
              {translate('sales.interaction.result')}
              <textarea
                required
                maxLength={10000}
                rows={3}
                value={form.result}
                onChange={(event) => setForm({ ...form, result: event.target.value })}
              />
            </label>
            <label>
              {translate('sales.interaction.nextAction')}
              <input
                maxLength={1000}
                value={form.next_action}
                onChange={(event) => setForm({ ...form, next_action: event.target.value })}
              />
            </label>
            <label>
              {translate('sales.interaction.nextDate')}
              <IsoDateInput
                value={form.next_action_date}
                onChange={(next_action_date) => setForm({ ...form, next_action_date })}
              />
            </label>
          </div>
          <p className="muted">
            {translate(form.revision ? 'sales.interaction.editHint' : 'sales.interaction.nextHint')}
          </p>
          {failure && (
            <p className="error" role="alert">
              {failure}
            </p>
          )}
          <div className="sales-interaction-actions">
            <button
              type="button"
              disabled={busy}
              onClick={() => {
                confirmDiscard(dirty, () => {
                  recovery.discard();
                  setForm(undefined);
                  setFailure('');
                });
              }}
            >
              {translate('action.cancel')}
            </button>
            <button className="primary" disabled={busy}>
              {translate(form.revision ? 'action.save' : 'sales.interaction.save')}
            </button>
          </div>
        </form>
      )}
      {form?.revision && (
        <section className="record-history" aria-label={translate('forms.changeHistory')}>
          <h3>{translate('forms.changeHistory')}</h3>
          {historyError && (
            <p className="error" role="alert">
              {historyError}
            </p>
          )}
          {history.map((entry, index) => (
            <RecordHistoryEntry
              key={entry.revision}
              initiallyOpen={index === history.length - 1}
              title={translate(entry.revision === 1 ? 'history.created' : 'history.updated')}
              actor={
                <EmployeeName
                  format="login"
                  subject={entry.actor.split('#').at(-1)}
                  session={session}
                />
              }
              occurredAt={entry.created_at}
              revision={translate('sales.revision', { revision: entry.revision })}
            >
              <RecordHistoryDiff
                rows={(
                  ['kind', 'contact_date', 'result', 'next_action', 'next_action_date'] as const
                )
                  .filter(
                    (field) =>
                      index === 0 || history[index - 1].snapshot[field] !== entry.snapshot[field],
                  )
                  .map((field) => ({
                    key: field,
                    label: translate(
                      (
                        {
                          kind: 'sales.interaction.kind',
                          contact_date: 'sales.interaction.date',
                          result: 'sales.interaction.result',
                          next_action: 'sales.interaction.nextAction',
                          next_action_date: 'sales.interaction.nextDate',
                        } as const
                      )[field],
                    ),
                    before: index
                      ? field === 'kind'
                        ? translate(`sales.interaction.${history[index - 1].snapshot.kind}`)
                        : (history[index - 1].snapshot[field] ?? '—')
                      : '—',
                    after:
                      field === 'kind'
                        ? translate(`sales.interaction.${entry.snapshot.kind}`)
                        : (entry.snapshot[field] ?? '—'),
                  }))}
              />
            </RecordHistoryEntry>
          ))}
        </section>
      )}
      {!form && failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {!loaded && !failure && <p>{translate('common.loading')}</p>}
      {loaded && items.length === 0 && (
        <p className="muted">{translate('sales.interaction.empty')}</p>
      )}
      <div className="sales-interaction-list">
        {items.map((item) => (
          <article
            className={`sales-interaction${session.permissions.includes('sales.write') && !form ? ' sales-interaction-editable' : ''}`}
            key={item.id}
            role={session.permissions.includes('sales.write') && !form ? 'button' : undefined}
            tabIndex={session.permissions.includes('sales.write') && !form ? 0 : undefined}
            aria-label={
              session.permissions.includes('sales.write') && !form
                ? `${translate('sales.interaction.edit')}: ${item.result}`
                : undefined
            }
            onClick={() => void edit(item)}
            onKeyDown={(event) => {
              if (event.key === 'Enter' || event.key === ' ') {
                event.preventDefault();
                void edit(item);
              }
            }}
          >
            <div className="sales-interaction-meta">
              <strong>{translate(`sales.interaction.${item.kind}`)}</strong>
              <time dateTime={item.contact_date}>
                {item.contact_date.split('-').reverse().join('.')}
              </time>
              <EmployeeName subject={item.author_subject} session={session} />
            </div>
            <p className="sales-interaction-result">{item.result}</p>
            {item.next_action && (
              <p className="muted">
                {translate('sales.interaction.nextAction')}: {item.next_action} ·{' '}
                {item.next_action_date?.split('-').reverse().join('.')}
              </p>
            )}
          </article>
        ))}
      </div>
      {before && (
        <button disabled={busy} onClick={() => void loadMore()}>
          {translate('sales.interaction.more')}
        </button>
      )}
    </section>
  );
}
