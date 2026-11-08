import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { RecordSection } from '../../../shared/ui/record/RecordLayout';
import { EmployeeName } from '../../../shared/profile/EmployeeName';
import { useLocalization } from '../../localization/Localization';
import { registryValueLabel } from '../../registry/registryLabels';
import type { Inquiry } from '../types';
import { RfqEditor } from './RfqEditor';
import { RfqDetails } from './RfqDetails';
import type { Rfq } from './types';
import './rfq.css';
export function SalesRfqs({ inquiry, session }: { inquiry: Inquiry; session: Session }) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<Rfq[]>([]);
  const [error, setError] = useState('');
  const [draft, setDraft] = useState<Rfq>();
  const write =
    session.permissions.includes('sales.write') && !inquiry.shipment_id && !inquiry.converting;
  useEffect(() => {
    const c = new AbortController();
    request<{ items: Rfq[] }>('/api/v1/sales/rfqs/list', session, { id: inquiry.id }, c.signal)
      .then((r) => setItems(r.items))
      .catch((e) => {
        if (!c.signal.aborted) setError(e.message);
      });
    return () => c.abort();
  }, [session, inquiry.id]);
  if (draft)
    return (
      <RfqEditor
        value={draft}
        inquiry={inquiry}
        session={session}
        onCancel={() => setDraft(undefined)}
        onSaved={(v) => {
          setItems((old) => [...old.filter((r) => r.input.id !== v.input.id), v]);
          setDraft(undefined);
        }}
      />
    );
  return (
    <div>
      <div className="proposal-toolbar">
        <h2>{translate('rfq.title')}</h2>
        {write && (
          <button
            className="primary"
            disabled={!inquiry.route_segments.length || items.length >= 100}
            onClick={() => {
              const segment = inquiry.route_segments[0];
              setDraft({
                revision: 0,
                inquiry_number: inquiry.number,
                route_segment: segment,
                input: {
                  id: crypto.randomUUID(),
                  inquiry_id: inquiry.id,
                  route_segment_id: segment.id,
                  title: '',
                  responsible_subject: session.subject,
                  status: 'in_progress',
                  quotes: [],
                },
              });
            }}
          >
            {translate('rfq.create')}
          </button>
        )}
      </div>
      {!inquiry.route_segments.length && <p className="hint">{translate('rfq.needRoute')}</p>}
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
      {!items.length && <p className="hint">{translate('rfq.empty')}</p>}
      {items.map((value) => (
        <RecordSection key={value.input.id} title={value.input.title}>
          <div className="rfq-actions">
            <a href={`#/rfqs/${value.input.id}`}>{translate('rfq.open')}</a>
            <span>
              {registryValueLabel('mode', value.route_segment.type, locale)} ·{' '}
              {translate(`rfq.status.${value.input.status}`)}
            </span>
            <EmployeeName session={session} subject={value.input.responsible_subject} />
            {value.input.answer_by && (
              <time>
                {new Date(`${value.input.answer_by}T12:00:00`).toLocaleDateString(locale)}
              </time>
            )}
          </div>
          <RfqDetails value={value} />
        </RecordSection>
      ))}
    </div>
  );
}
