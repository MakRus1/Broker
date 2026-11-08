import { useEffect, useRef, useState } from 'react';
import type { Session } from '../../../shared/api/session';
import { request } from '../../../shared/api/http';
import { RecordHeading, RecordSection } from '../../../shared/ui/record/RecordLayout';
import { RecordHistoryEntry, RecordHistoryDiff } from '../../../shared/ui/record/RecordHistory';
import { EmployeeName } from '../../../shared/profile/EmployeeName';
import { useLocalization } from '../../localization/Localization';
import type { Inquiry } from '../types';
import type { Rfq } from './types';
import { RfqEditor } from './RfqEditor';
import { RfqDetails } from './RfqDetails';
import './rfq.css';
type History = { snapshot: Rfq; actor: string; created_at: string };
export function RfqPage({
  route,
  session,
  onTitle,
}: {
  route: string;
  session: Session;
  onTitle: (title: string) => void;
}) {
  const id = route.split('/')[2];
  const { translate } = useLocalization();
  const [value, setValue] = useState<Rfq>();
  const [inquiry, setInquiry] = useState<Inquiry>();
  const [editing, setEditing] = useState(false);
  const [history, setHistory] = useState<History[]>([]);
  const [tab, setTab] = useState('main');
  const [error, setError] = useState('');
  const title = useRef(onTitle);
  title.current = onTitle;
  useEffect(() => {
    const c = new AbortController();
    request<{ item: Rfq }>('/api/v1/sales/rfqs/get', session, { id }, c.signal)
      .then(async ({ item }) => {
        setValue(item);
        title.current(item.input.title);
        const result = await request<{ item: Inquiry }>(
          '/api/v1/sales/inquiries/get',
          session,
          { id: item.input.inquiry_id },
          c.signal,
        );
        setInquiry(result.item);
      })
      .catch((e) => {
        if (!c.signal.aborted) setError(e.message);
      });
    return () => c.abort();
  }, [session, id]);
  async function showHistory() {
    setTab('history');
    try {
      const result = await request<{ items: History[] }>('/api/v1/sales/rfqs/history', session, {
        id,
      });
      setHistory(result.items);
    } catch (e) {
      setError((e as Error).message);
    }
  }
  return (
    <section className="surface record-workbench sales-workbench">
      <RecordHeading
        kind={translate('rfq.title')}
        title={value?.input.title ?? translate('common.loading')}
        status={value ? translate(`rfq.status.${value.input.status}`) : ''}
        metadata={
          value && (
            <>
              <a href={`#/inquiries/${value.input.inquiry_id}`}>{value.inquiry_number}</a>
              <span> · {translate('sales.revision', { revision: value.revision })}</span>
            </>
          )
        }
        actions={
          !editing &&
          inquiry &&
          !inquiry.shipment_id &&
          !inquiry.converting &&
          session.permissions.includes('sales.write') ? (
            <button className="primary" onClick={() => setEditing(true)}>
              {translate('sales.edit')}
            </button>
          ) : undefined
        }
      />
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      {value &&
        inquiry &&
        (editing ? (
          <RfqEditor
            key={value.revision}
            value={value}
            inquiry={inquiry}
            session={session}
            onCancel={() => setEditing(false)}
            onSaved={(v) => {
              setValue(v);
              title.current(v.input.title);
              setEditing(false);
            }}
          />
        ) : (
          <>
            <div className="tabs shipment-tabs record-layout-tabs" role="tablist">
              <button role="tab" aria-selected={tab === 'main'} onClick={() => setTab('main')}>
                {translate('sales.main')}
              </button>
              <button
                role="tab"
                aria-selected={tab === 'history'}
                onClick={() => void showHistory()}
              >
                {translate('sales.history')}
              </button>
            </div>
            {tab === 'main' ? (
              <RecordSection title={translate('rfq.quotes')}>
                <p>
                  <EmployeeName session={session} subject={value.input.responsible_subject} />
                </p>
                <RfqDetails value={value} />
              </RecordSection>
            ) : (
              history.map((entry, i) => (
                <RecordHistoryEntry
                  key={entry.snapshot.revision}
                  title={translate('rfq.title')}
                  actor={entry.actor.split('#').at(-1)}
                  occurredAt={entry.created_at}
                  revision={translate('sales.revision', { revision: entry.snapshot.revision })}
                  initiallyOpen={i === history.length - 1}
                >
                  <RecordHistoryDiff
                    rows={(['title', 'responsible_subject', 'status', 'answer_by', 'note'] as const)
                      .filter(
                        (key) =>
                          i === 0 ||
                          entry.snapshot.input[key] !== history[i - 1].snapshot.input[key],
                      )
                      .map((key) => ({
                        key,
                        label: translate(
                          key === 'status'
                            ? 'registryField.status'
                            : key === 'title'
                              ? 'proposal.name'
                              : key === 'responsible_subject'
                                ? 'rfq.responsible'
                                : key === 'answer_by'
                                  ? 'rfq.answerBy'
                                  : 'sales.note',
                        ),
                        before: i > 0 ? (history[i - 1].snapshot.input[key] ?? '—') : '—',
                        after: entry.snapshot.input[key] ?? '—',
                      }))}
                  />
                  <RfqDetails value={entry.snapshot} />
                </RecordHistoryEntry>
              ))
            )}
          </>
        ))}
    </section>
  );
}
