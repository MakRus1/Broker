import './settlements.css';

import { useEffect, useMemo, useState, type FormEvent } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
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
const views: { value: SettlementView; label: string }[] = [
  { value: 'outgoing', label: 'Исходящие счета' },
  { value: 'incoming', label: 'Входящие счета' },
  { value: 'receipt', label: 'Поступления' },
  { value: 'disbursement', label: 'Списания' },
];
export function CounterpartySettlements({
  counterpartyId,
  session,
  version,
}: {
  counterpartyId: string;
  session: Session;
  version: number;
}) {
  const [draft, setDraft] = useState(empty);
  const [contractName, setContractName] = useState('');
  const [appliedContractName, setAppliedContractName] = useState('');
  const [filters, setFilters] = useState(empty);
  const [totals, setTotals] = useState<SettlementTotal[]>([]);
  const [loading, setLoading] = useState(true);
  const [failure, setFailure] = useState('');
  const [view, setView] = useState<SettlementView>('outgoing');
  const [overdue, setOverdue] = useState(false);
  useEffect(() => {
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
  }, [counterpartyId, filters, session, version]);
  const scope = useMemo(
    () => settlementScope(counterpartyId, filters, view, overdue),
    [counterpartyId, filters, view, overdue],
  );
  function apply(event: FormEvent) {
    event.preventDefault();
    setFilters({ ...draft });
    setAppliedContractName(contractName);
  }
  function drillDown(next: SettlementView, currency: string, onlyOverdue = false) {
    setView(next);
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
    <section className="counterparty-settlements" aria-label="Взаиморасчёты">
      <form className="settlement-filters" onSubmit={apply}>
        <CustomerSelect
          items={[]}
          label="Своё юрлицо"
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
            session={session}
            kind="contracts"
            label="Договор"
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
          label="Валюта"
          selectedLabel={draft.currency || undefined}
          value={draft.currency}
          options={[{ value: '', label: 'Все валюты' }, ...currenciesWithCurrent(draft.currency)]}
          onChange={(currency) => setDraft({ ...draft, currency })}
        />
        <div className="settlement-filter-actions">
          <button
            type="button"
            onClick={() => {
              setDraft(empty);
              setFilters(empty);
              setContractName('');
              setAppliedContractName('');
              setOverdue(false);
            }}
          >
            Сбросить
          </button>
          <button className="primary" type="submit">
            Применить
          </button>
        </div>
      </form>
      {failure ? (
        <p className="error" role="alert">
          {failure}
        </p>
      ) : loading ? (
        <p className="hint" role="status">
          Считаем взаиморасчёты…
        </p>
      ) : totals.length === 0 ? (
        <p className="hint">Счетов и платежей по выбранным условиям нет.</p>
      ) : (
        <>
          <div className="settlement-table-wrap">
            <table className="settlement-totals" aria-label="Текущие взаиморасчёты по валютам">
              <thead>
                <tr className="settlement-groups">
                  <th rowSpan={2} scope="col">
                    Валюта
                  </th>
                  <th colSpan={2} scope="colgroup">
                    Нам должны
                  </th>
                  <th colSpan={2} scope="colgroup">
                    Мы должны
                  </th>
                  <th colSpan={2} scope="colgroup">
                    Нераспределено
                  </th>
                </tr>
                <tr>
                  <th scope="col">Всего</th>
                  <th scope="col">Просрочено</th>
                  <th scope="col">Всего</th>
                  <th scope="col">Просрочено</th>
                  <th scope="col">Поступления</th>
                  <th scope="col">Списания</th>
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
                    <td>{amount(total.overdue_payable_minor, total.currency, 'incoming', true)}</td>
                    <td>{amount(total.unallocated_receipts_minor, total.currency, 'receipt')}</td>
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
            Нераспределённые платежи не привязаны к договору и не вычитаются из долга.
          </p>
        </>
      )}
      <div className="settlement-detail-heading">
        <div className="tabs" role="group" aria-label="Документы взаиморасчётов">
          {views.map((option) => (
            <button
              key={option.value}
              type="button"
              aria-pressed={view === option.value}
              onClick={() => {
                setView(option.value);
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
            Только просроченные
          </label>
        )}
      </div>
      {view === 'outgoing' || view === 'incoming' ? (
        <InvoiceRegistry
          key={view + JSON.stringify(scope.filters)}
          direction={view}
          active
          session={session}
          scope={scope}
          refreshVersion={version}
        />
      ) : (
        <PaymentRegistry
          key={view + JSON.stringify(scope.filters)}
          kind={view}
          active
          session={session}
          scope={scope}
          refreshVersion={version}
        />
      )}
    </section>
  );
}
