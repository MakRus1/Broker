import { useLocalization } from '../../localization/Localization';
import './settlements.css';

import { useEffect, useMemo, useRef, useState, type FormEvent } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { DateInput } from '../../../shared/dates/DateInput';
import { CounterpartyReconciliation } from '../reconciliation/CounterpartyReconciliation';
import { Select } from '../../../shared/ui/Select';
import { CustomerSelect } from '../../counterparties/CustomerSelect';
import { ServiceChargeLookup } from '../invoices/ServiceChargeLookup';
import type { Contract } from '../../contracts/ContractsPage';
import { currenciesWithCurrent } from '../currencies';
import { formatFixed } from '../services/serviceCharges';
import { InvoiceRegistry } from '../invoices/InvoiceRegistry';
import { PaymentRegistry } from '../payments/PaymentRegistry';
import {
  settlementScope,
  type SettlementFilters,
  type SettlementTotal,
  type SettlementView,
} from './settlements';

const empty: SettlementFilters = { own_company_id: '', contract_id: '', currency: '' };
export function CounterpartySettlements({
  counterpartyId,
  counterpartyName,
  session,
  version,
}: {
  counterpartyId: string;
  counterpartyName: string;
  session: Session;
  version: number;
}) {
  const { translate, locale } = useLocalization();
  const views: { value: SettlementView; label: string }[] = [
    { value: 'outgoing', label: translate('nav.outgoingInvoices') },
    { value: 'incoming', label: translate('nav.incomingInvoices') },
    { value: 'receipt', label: translate('nav.receipts') },
    { value: 'disbursement', label: translate('nav.disbursements') },
  ];
  const [mode, setMode] = useState('balances');
  const [initialPeriod] = useState(() => ({
    from: CalendarDate.fromISO(`${new Date().getFullYear()}-01-01`),
    to: CalendarDate.fromISO(new Date().toLocaleDateString('sv-SE')),
  }));
  const [from, setFrom] = useState<CalendarDate | null>(initialPeriod.from);
  const [to, setTo] = useState<CalendarDate | null>(initialPeriod.to);
  const [period, setPeriod] = useState(initialPeriod);
  const [periodError, setPeriodError] = useState('');
  const [draft, setDraft] = useState(empty);
  const [contractName, setContractName] = useState('');
  const [appliedContractName, setAppliedContractName] = useState('');
  const [filters, setFilters] = useState(empty);
  const [totals, setTotals] = useState<SettlementTotal[]>([]);
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [view, setView] = useState<SettlementView>('outgoing');
  const [openedViews, setOpenedViews] = useState<SettlementView[]>(['outgoing']);
  function selectDocumentView(next: SettlementView) {
    setOpenedViews((old) => (old.includes(next) ? old : [...old, next]));
    setView(next);
  }
  const [overdue, setOverdue] = useState(false);
  const documents = useRef<HTMLDivElement>(null);
  const [documentsMinHeight, setDocumentsMinHeight] = useState(0);
  function preserveDocumentViewport() {
    if (!documents.current) return;
    const bounds = documents.current.getBoundingClientRect();
    // При смене реестра его строки исчезают до ответа сервера. Сохраняем
    // занятую часть экрана, чтобы браузер не уменьшал scrollY вместе со страницей.
    setDocumentsMinHeight(Math.max(0, Math.min(bounds.height, window.innerHeight - bounds.top)));
  }
  useEffect(() => {
    if (mode !== 'balances') return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setTotals([]);
    request<{ totals: SettlementTotal[] }>(
      '/api/v1/counterparty-settlements/get',
      session,
      {
        counterparty_id: counterpartyId,
        own_company_id: filters.own_company_id || undefined,
        contract_id: filters.contract_id || undefined,
        currency: filters.currency || undefined,
      },
      controller.signal,
    )
      .then((response) => {
        if (!controller.signal.aborted) setTotals(response.totals);
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [counterpartyId, filters, session, version, mode]);
  function apply(event: FormEvent) {
    event.preventDefault();
    if (mode === 'reconciliation') {
      if (!from || !to || from.toString() > to.toString()) {
        setPeriodError(translate('forms.enterAValidPeriodStartAndEnd'));
        return;
      }
      setPeriod({ from, to });
      setPeriodError('');
    }
    setFilters({ ...draft });
    setAppliedContractName(contractName);
  }
  function drillDown(next: SettlementView, currency: string, onlyOverdue = false) {
    preserveDocumentViewport();
    selectDocumentView(next);
    setOverdue(onlyOverdue);
    setDraft({ ...filters, currency });
    setContractName(appliedContractName);
    setFilters({ ...filters, currency });
  }
  function amount(value: string, currency: string, next: SettlementView, late = false) {
    if (BigInt(value) === 0n) return <span className="settlement-zero">0,00</span>;
    return (
      <button className="link settlement-amount" onClick={() => drillDown(next, currency, late)}>
        {formatFixed(value, 2)}
      </button>
    );
  }
  return (
    <section className="counterparty-settlements" aria-label={translate('forms.settlements')}>
      <div className="tabs" role="group" aria-label={translate('forms.settlementView')}>
        <button aria-pressed={mode === 'balances'} onClick={() => setMode('balances')}>
          {translate('forms.outstandingBalances')}
        </button>
        <button aria-pressed={mode === 'reconciliation'} onClick={() => setMode('reconciliation')}>
          {translate('forms.reconciliationStatement')}
        </button>
      </div>
      <form
        className={`settlement-filters ${mode === 'reconciliation' ? 'reconciliation-filters' : ''}`}
        onSubmit={apply}
      >
        <CustomerSelect
          allowCreate={false}
          items={[]}
          label={translate('forms.ownLegalEntityLabel')}
          ownCompany
          disabled={false}
          session={session}
          value={draft.own_company_id}
          onChange={(own_company_id) => {
            setDraft({ ...draft, own_company_id, contract_id: '' });
            setContractName('');
          }}
        />
        {session.permissions.includes('contracts.read') && (
          <ServiceChargeLookup<Contract>
            allowCreate={false}
            session={session}
            kind="contracts"
            label={translate('registryField.contract_id')}
            value={draft.contract_id}
            name={contractName}
            counterparty={counterpartyId}
            disabled={false}
            onChange={(contract) => {
              setDraft({ ...draft, contract_id: contract?.id ?? '' });
              setContractName(contract?.number ?? '');
            }}
          />
        )}
        <Select
          label={translate('registryField.currency')}
          selectedLabel={draft.currency || undefined}
          value={draft.currency}
          options={[
            { value: '', label: translate('forms.allCurrencies') },
            ...currenciesWithCurrent(draft.currency, locale),
          ]}
          onChange={(currency) => setDraft({ ...draft, currency })}
        />
        {mode === 'reconciliation' && (
          <>
            <label>
              {translate('forms.periodFrom')}
              <DateInput required value={from} onChange={setFrom} />
            </label>
            <label>
              {translate('filter.to')}
              <DateInput required value={to} onChange={setTo} />
            </label>
          </>
        )}
        <div className="settlement-filter-actions">
          <button
            type="button"
            onClick={() => {
              setDraft(empty);
              setFilters(empty);
              setContractName('');
              setAppliedContractName('');
              setOverdue(false);
              setFrom(initialPeriod.from);
              setTo(initialPeriod.to);
              setPeriod(initialPeriod);
              setPeriodError('');
            }}
          >
            {translate('forms.reset')}
          </button>
          <button className="primary" type="submit">
            {mode === 'reconciliation' ? translate('forms.generate') : translate('filter.apply')}
          </button>
        </div>
      </form>
      {periodError && (
        <p className="error" role="alert">
          {periodError}
        </p>
      )}
      {mode === 'reconciliation' ? (
        <CounterpartyReconciliation
          counterpartyId={counterpartyId}
          counterpartyName={counterpartyName}
          filters={filters}
          from={period.from}
          to={period.to}
          contractName={appliedContractName}
          session={session}
          version={version}
        />
      ) : (
        <>
          {failure ? (
            <p className="error" role="alert">
              {failure}
            </p>
          ) : loading ? (
            <p className="hint" role="status">
              {translate('forms.calculatingSettlements')}
            </p>
          ) : totals.length === 0 ? (
            <p className="hint">{translate('forms.noInvoicesOrPaymentsMatchTheseConditions')}</p>
          ) : (
            <>
              <div className="settlement-table-wrap">
                <table
                  className="settlement-totals"
                  aria-label={translate('forms.currentSettlementsByCurrency')}
                >
                  <thead>
                    <tr className="settlement-groups">
                      <th rowSpan={2} scope="col">
                        {translate('registryField.currency')}
                      </th>
                      <th colSpan={2} scope="colgroup">
                        {translate('forms.receivables')}
                      </th>
                      <th colSpan={2} scope="colgroup">
                        {translate('forms.payables')}
                      </th>
                      <th colSpan={2} scope="colgroup">
                        {translate('forms.unallocated')}
                      </th>
                    </tr>
                    <tr>
                      <th scope="col">{translate('finance.totalLabel')}</th>
                      <th scope="col">{translate('forms.overdue')}</th>
                      <th scope="col">{translate('finance.totalLabel')}</th>
                      <th scope="col">{translate('forms.overdue')}</th>
                      <th scope="col">{translate('nav.receipts')}</th>
                      <th scope="col">{translate('nav.disbursements')}</th>
                    </tr>
                  </thead>
                  <tbody>
                    {totals.map((total) => (
                      <tr key={total.currency}>
                        <th scope="row">{total.currency}</th>
                        <td>{amount(total.receivable_minor, total.currency, 'outgoing')}</td>
                        <td>
                          {amount(total.overdue_receivable_minor, total.currency, 'outgoing', true)}
                        </td>
                        <td>{amount(total.payable_minor, total.currency, 'incoming')}</td>
                        <td>
                          {amount(total.overdue_payable_minor, total.currency, 'incoming', true)}
                        </td>
                        <td>
                          {amount(total.unallocated_receipts_minor, total.currency, 'receipt')}
                        </td>
                        <td>
                          {amount(
                            total.unallocated_disbursements_minor,
                            total.currency,
                            'disbursement',
                          )}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <p className="hint settlement-explanation">
                {translate('settlements.unallocatedHint')}
              </p>
            </>
          )}
          <div ref={documents} style={{ minHeight: documentsMinHeight }}>
            <div className="settlement-detail-heading">
              <div
                className="tabs"
                role="group"
                aria-label={translate('forms.settlementDocuments')}
              >
                {views.map((option) => (
                  <button
                    key={option.value}
                    type="button"
                    aria-pressed={view === option.value}
                    onClick={() => {
                      preserveDocumentViewport();
                      selectDocumentView(option.value);
                      setOverdue(false);
                    }}
                  >
                    {option.label}
                  </button>
                ))}
              </div>
              {(view === 'outgoing' || view === 'incoming') && (
                <label className="settlement-overdue">
                  <input
                    type="checkbox"
                    checked={overdue}
                    onChange={(event) => setOverdue(event.target.checked)}
                  />
                  {translate('forms.overdueOnly')}
                </label>
              )}
            </div>
            {openedViews.map((openedView) => (
              <div key={openedView} hidden={view !== openedView}>
                <SettlementDocuments
                  view={openedView}
                  counterpartyId={counterpartyId}
                  filters={filters}
                  overdue={overdue}
                  session={session}
                  version={version}
                />
              </div>
            ))}
          </div>
        </>
      )}
    </section>
  );
}

// Посещённые реестры сохраняют данные и настройки при переключении вкладок.
// Первый запрос выполняется только при первом открытии конкретного реестра.
function SettlementDocuments({
  view,
  counterpartyId,
  filters,
  overdue,
  session,
  version,
}: {
  view: SettlementView;
  counterpartyId: string;
  filters: SettlementFilters;
  overdue: boolean;
  session: Session;
  version: number;
}) {
  const scope = useMemo(
    () => settlementScope(counterpartyId, filters, view, overdue),
    [counterpartyId, filters, view, overdue],
  );
  return view === 'outgoing' || view === 'incoming' ? (
    <InvoiceRegistry
      direction={view}
      active
      session={session}
      scope={scope}
      refreshVersion={version}
    />
  ) : (
    <PaymentRegistry kind={view} active session={session} scope={scope} refreshVersion={version} />
  );
}
