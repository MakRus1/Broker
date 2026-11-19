import { quotationLines } from './quotationImport';
import { importedQuotationKeys, quotationLineKey } from './quotationComparison';
import { RfqDetails } from './RfqDetails';
import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useEffect, useState } from 'react';
import type { Session } from '../../../shared/api/session';
import { request } from '../../../shared/api/http';
import { useLocalization } from '../../localization/Localization';
import type { Inquiry } from '../types';
import type { ProposalLine } from '../proposals';
import type { Rfq } from './types';
import './rfq.css';

export function RfqQuotePicker({
  inquiry,
  session,
  lines,
  calculationDate,
  onAdd,
}: {
  inquiry: Inquiry;
  session: Session;
  lines: ProposalLine[];
  calculationDate: string;
  onAdd: (lines: ProposalLine[], names: Record<string, string>) => void;
}) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<Rfq[]>([]);
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(true);
  const added = importedQuotationKeys(lines);
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setError('');
    request<{ items: Rfq[] }>(
      '/api/v1/sales/rfqs/list',
      session,
      { id: inquiry.id },
      controller.signal,
    )
      .then(({ items }) => {
        if (!controller.signal.aborted) setItems(items.filter((v) => v.input.status === 'ready'));
      })
      .catch((e) => {
        if (!controller.signal.aborted) setError(e.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [session, inquiry.id]);

  const chosen = items.flatMap((rfq) =>
    rfq.input.quotes.flatMap((quote) => {
      if (quote.valid_until && quote.valid_until < calculationDate) return [];
      const available = quote.lines.filter((line) => {
        const key = quotationLineKey(rfq, quote, line);
        return selected.has(key) && !added.has(key);
      });
      return available.length ? [{ rfq, quote, lines: available }] : [];
    }),
  );
  const count = chosen.reduce((sum, value) => sum + value.lines.length, 0);

  return (
    <details className="rfq-import">
      <summary>{translate('rfq.import')}</summary>
      <div className="rfq-import-body">
        <p className="hint">{translate('rfq.importHint')}</p>
        {loading && <p role="status">{translate('common.loading')}</p>}
        {error && (
          <p className="error" role="alert">
            {error}
          </p>
        )}
        {!loading && !error && !items.length && (
          <p className="hint">{translate('rfq.noReadyQuotes')}</p>
        )}
        {items.map((rfq) => (
          <section key={rfq.input.id} className="rfq-import-request">
            <h3>{rfq.input.title}</h3>
            <p className="hint">{sharedRouteDirection(rfq.route_segment, locale)}</p>
            <RfqDetails
              value={rfq}
              showRoute={false}
              selection={{
                selected,
                added,
                calculationDate,
                onToggle: (keys, checked) =>
                  setSelected((previous) => {
                    const next = new Set(previous);
                    for (const key of keys) {
                      if (checked) next.add(key);
                      else next.delete(key);
                    }
                    return next;
                  }),
              }}
            />
          </section>
        ))}
        {!!items.length && (
          <div className="rfq-import-actions">
            <span aria-live="polite">{translate('rfq.selectedCount', { count })}</span>
            <button
              type="button"
              disabled={!count || loading || !!error}
              onClick={() => {
                onAdd(
                  chosen.flatMap((value) => quotationLines(value.rfq, value.quote, value.lines)),
                  Object.fromEntries(
                    chosen.flatMap((value) =>
                      value.lines.map((line) => [line.label_id, line.label_name ?? '']),
                    ),
                  ),
                );
                setSelected(new Set());
              }}
            >
              {translate('rfq.addToProposal')}
            </button>
          </div>
        )}
      </div>
    </details>
  );
}
