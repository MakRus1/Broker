import { sharedRouteDirection } from '../../routes/SharedRouteDetails';
import { quotationLines } from './quotationImport';
import { useEffect, useState } from 'react';
import type { Session } from '../../../shared/api/session';
import { request } from '../../../shared/api/http';
import { Select } from '../../../shared/ui/Select';
import { formatFixed } from '../../finance/services/serviceCharges';
import { useLocalization } from '../../localization/Localization';
import type { Inquiry } from '../types';
import type { ProposalLine } from '../proposals';
import type { Rfq } from './types';
export function RfqQuotePicker({
  inquiry,
  session,
  onAdd,
}: {
  inquiry: Inquiry;
  session: Session;
  onAdd: (lines: ProposalLine[], names: Record<string, string>) => void;
}) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<Rfq[]>([]);
  const [selected, setSelected] = useState('');
  const [error, setError] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    request<{ items: Rfq[] }>(
      '/api/v1/sales/rfqs/list',
      session,
      { id: inquiry.id },
      controller.signal,
    )
      .then(({ items }) => setItems(items.filter((v) => v.input.status === 'ready')))
      .catch((e) => {
        if (!controller.signal.aborted) setError(e.message);
      });
    return () => controller.abort();
  }, [session, inquiry.id]);
  const choices = items.flatMap((rfq) =>
    rfq.input.quotes.map((quote) => ({ rfq, quote, value: `${rfq.input.id}:${quote.id}` })),
  );
  return (
    <div className="rfq-import">
      <p className="rfq-import-hint hint">{translate('rfq.importHint')}</p>
      <Select
        label={translate('rfq.import')}
        value={selected}
        options={[
          { value: '', label: translate('rfq.chooseQuote') },
          ...choices.map(({ rfq, quote, value }) => ({
            value,
            label: `${sharedRouteDirection(rfq.route_segment, locale)} · ${rfq.input.title} · ${quote.supplier_name ?? ''} · ${quote.title} · ${quote.lines.map((l) => `${formatFixed(l.price.price_minor, 2)} ${l.price.currency}`).join(' + ')}`,
          })),
        ]}
        onChange={setSelected}
      />
      <button
        type="button"
        disabled={!selected}
        onClick={() => {
          const choice = choices.find((v) => v.value === selected);
          if (!choice) return;
          const { rfq, quote } = choice;
          onAdd(
            quotationLines(rfq, quote),
            Object.fromEntries(quote.lines.map((line) => [line.label_id, line.label_name ?? ''])),
          );
          setSelected('');
        }}
      >
        {translate('rfq.addToProposal')}
      </button>
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
    </div>
  );
}
