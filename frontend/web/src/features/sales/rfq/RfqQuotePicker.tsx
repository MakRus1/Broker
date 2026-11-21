import { quotationLines } from './quotationImport';
import {
  importedQuotationKeys,
  quotationLineKey,
  quotationTotals,
  quotationServiceKey,
  selectQuotationAlternatives,
} from './quotationComparison';
import { RfqServiceComparison } from './RfqServiceComparison';
import { formatFixed } from '../../finance/services/serviceCharges';
import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { useEffect, useRef, useState } from 'react';
import type { Session } from '../../../shared/api/session';
import { request } from '../../../shared/api/http';
import { useLocalization } from '../../localization/Localization';
import type { Inquiry } from '../types';
import type { ProposalLine } from '../proposals';
import type { Rfq, ProposalSuggestionResult } from './types';
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
  const root = useRef<HTMLElement>(null);
  useEffect(() => {
    const heading = root.current
      ?.closest('.record-workbench')
      ?.querySelector('.record-layout-heading');
    if (!heading) return;
    const update = () =>
      root.current?.style.setProperty(
        '--rfq-summary-top',
        `${heading.getBoundingClientRect().height + 16}px`,
      );
    const observer = new ResizeObserver(update);
    observer.observe(heading);
    update();
    return () => observer.disconnect();
  }, []);
  const [items, setItems] = useState<Rfq[]>([]);
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [error, setError] = useState('');
  const [automatic, setAutomatic] = useState(false);
  const [suggesting, setSuggesting] = useState(false);
  const [suggestion, setSuggestion] = useState<ProposalSuggestionResult>();
  const suggestionAbort = useRef<AbortController | undefined>(undefined);
  useEffect(() => {
    setAutomatic(false);
    setSuggesting(false);
    suggestionAbort.current?.abort();
    return () => suggestionAbort.current?.abort();
  }, [inquiry.id, calculationDate, session]);
  async function suggest() {
    suggestionAbort.current?.abort();
    const controller = new AbortController();
    suggestionAbort.current = controller;
    setSuggesting(true);
    setAutomatic(false);
    setError('');
    try {
      const result = await request<ProposalSuggestionResult>(
        '/api/v1/sales/proposals/suggest',
        session,
        { inquiry_id: inquiry.id, calculation_date: calculationDate, strategy: 'minimum_purchase' },
        controller.signal,
      );
      if (controller.signal.aborted) return;
      setItems(result.items);
      setSelected(
        new Set(
          result.selected.map(
            (source) => `${source.rfq_id}:${source.revision}:${source.quote_id}:${source.line_id}`,
          ),
        ),
      );
      setSuggestion(result);
      setAutomatic(true);
    } catch (e) {
      if (!controller.signal.aborted) setError((e as Error).message);
    } finally {
      if (!controller.signal.aborted) setSuggesting(false);
    }
  }
  const [loading, setLoading] = useState(true);
  const alreadyImported = importedQuotationKeys(lines);
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
        return selected.has(key) && !alreadyImported.has(key);
      });
      return available.length ? [{ rfq, quote, lines: available }] : [];
    }),
  );
  const pendingServices = new Set(
    chosen.flatMap(({ rfq, lines }) =>
      lines.map((line) => quotationServiceKey(rfq.input.route_segment_id, line.label_id)),
    ),
  );
  const replaced = lines.filter(
    (line) =>
      line.quote_source &&
      pendingServices.has(quotationServiceKey(line.route_segment_id, line.label_id)),
  );
  const retained = lines.filter((line) => !replaced.includes(line));
  const added = importedQuotationKeys(retained);
  const labelNames = new Map(
    items.flatMap((rfq) =>
      rfq.input.quotes.flatMap((quote) =>
        quote.lines.map((line) => [line.label_id, line.label_name] as const),
      ),
    ),
  );
  const count = chosen.reduce((sum, value) => sum + value.lines.length, 0);

  const totals = quotationTotals([
    ...retained.flatMap((line) => (line.purchase ? [{ ...line, price: line.purchase }] : [])),
    ...chosen.flatMap((value) => value.lines),
  ]);
  const routePosition = new Map(
    inquiry.route_segments.map((segment, index) => [segment.id, index]),
  );
  const ordered = [...items].sort(
    (a, b) =>
      (routePosition.get(a.input.route_segment_id) ?? Infinity) -
      (routePosition.get(b.input.route_segment_id) ?? Infinity),
  );
  return (
    <section ref={root} className="rfq-workbench">
      <div className="rfq-workbench-intro">
        <p className="hint">{translate('rfq.importHint')}</p>
        <div className="rfq-purchase-toolbar">
          <button type="button" disabled={loading || suggesting} onClick={() => void suggest()}>
            {translate(suggesting ? 'common.loading' : 'rfq.minimumPurchase')}
          </button>
          <details className="rfq-purchase-help">
            <summary>{translate('rfq.minimumPurchaseHow')}</summary>
            <p className="hint">{translate('rfq.minimumPurchaseHint')}</p>
          </details>
        </div>
      </div>
      {automatic && suggestion && (
        <div className="rfq-automatic-result" role="status">
          <p>{translate('rfq.minimumPurchaseResult', { count: suggestion.selected.length })}</p>
          {!!suggestion.gaps.length && (
            <>
              <strong>{translate('rfq.purchaseGaps')}</strong>
              <ul>
                {suggestion.gaps.map((gap) => (
                  <li key={`${gap.segment}:${gap.name}`}>
                    {translate('proposal.compareSegment', {
                      number: (routePosition.get(gap.segment) ?? -1) + 1,
                    })}
                    {gap.name && ` · ${gap.name}`}: {translate(`rfq.purchaseGap.${gap.reason}`)}
                  </li>
                ))}
              </ul>
            </>
          )}
          {!suggestion.gaps.length && <p>{translate('rfq.purchaseCoverageHint')}</p>}
        </div>
      )}

      <div className="rfq-workbench-layout">
        <fieldset className="rfq-workbench-options" disabled={suggesting}>
          {loading && <p role="status">{translate('common.loading')}</p>}
          {error && (
            <p className="error" role="alert">
              {error}
            </p>
          )}
          {!loading && !error && !items.length && (
            <p className="hint">{translate('rfq.noReadyQuotes')}</p>
          )}
          {ordered.map((rfq) => (
            <section key={rfq.input.id} className="rfq-import-request">
              <h3>{rfq.input.title}</h3>
              <p className="hint">{sharedRouteDirection(rfq.route_segment, locale)}</p>
              <RfqServiceComparison
                value={rfq}
                selection={{
                  selected,
                  added,
                  calculationDate,
                  onToggle: (keys, checked) => {
                    setAutomatic(false);
                    setSelected((previous) =>
                      selectQuotationAlternatives(previous, items, keys, checked),
                    );
                  },
                }}
              />
            </section>
          ))}
        </fieldset>
        <aside className="rfq-selection-summary" aria-label={translate('rfq.composition')}>
          <h3>{translate('rfq.composition')}</h3>
          <p>{translate('rfq.alreadyCount', { count: lines.length })}</p>
          <div className="rfq-selected-lines">
            {!!lines.length && (
              <ul className="rfq-existing-lines">
                {lines.map((line) => (
                  <li key={line.id} data-replaced={replaced.includes(line)}>
                    <span>
                      {replaced.includes(line) ? translate('rfq.willReplace') : '✓'}{' '}
                      {labelNames.get(line.label_id) ?? translate('proposal.service')}
                    </span>
                    {line.purchase && <small>{line.purchase.currency}</small>}
                  </li>
                ))}
              </ul>
            )}
            {chosen.map(({ rfq, quote, lines: selectedLines }) => (
              <section key={`${rfq.input.id}:${quote.id}`}>
                <h4>{rfq.input.title}</h4>
                <small>
                  {quote.supplier_name} · {quote.title}
                </small>
                <ul>
                  {selectedLines.map((line) => (
                    <li key={line.id}>
                      <span>{line.label_name ?? line.label_id}</span>
                      <button
                        type="button"
                        disabled={suggesting}
                        aria-label={translate('rfq.removeSelection', {
                          service: line.label_name ?? line.label_id,
                        })}
                        onClick={() => {
                          setAutomatic(false);
                          setSelected((old) => {
                            const next = new Set(old);
                            next.delete(quotationLineKey(rfq, quote, line));
                            return next;
                          });
                        }}
                      >
                        ×
                      </button>
                    </li>
                  ))}
                </ul>
              </section>
            ))}
            {!count && <p className="hint">{translate('rfq.chooseHint')}</p>}
          </div>
          <div className="rfq-selection-totals" aria-live="polite">
            <strong>{translate('rfq.purchaseTotal')}</strong>
            {totals.map((total) => (
              <div key={total.currency}>
                <span>{total.currency}</span>
                <strong>{formatFixed(total.total, 2)}</strong>
              </div>
            ))}
            {!!totals.length && <small>{translate('rfq.purchaseTotalHint')}</small>}
          </div>
          <span aria-live="polite">{translate('rfq.selectedCount', { count })}</span>
          {!!replaced.length && (
            <p className="hint">{translate('rfq.replaceCount', { count: replaced.length })}</p>
          )}
          <button
            type="button"
            className="primary"
            disabled={!count || loading || suggesting || !!error}
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
              setAutomatic(false);
            }}
          >
            {translate(replaced.length ? 'rfq.applySelection' : 'rfq.addToProposal')}
          </button>
        </aside>
      </div>
    </section>
  );
}
