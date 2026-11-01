import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { formatFixed } from '../services/serviceCharges';
import type { SettlementFilters } from '../settlements/settlements';
import {
  reconciliationCsv,
  reconciliationLabels,
  reconciliationLink,
  type Reconciliation,
  type ReconciliationContext,
} from './reconciliation';
import './reconciliation.css';
export function CounterpartyReconciliation({
  counterpartyId,
  counterpartyName,
  filters,
  from,
  to,
  contractName,
  session,
  version,
}: {
  counterpartyId: string;
  counterpartyName: string;
  filters: SettlementFilters;
  from: CalendarDate;
  to: CalendarDate;
  contractName: string;
  session: Session;
  version: number;
}) {
  const [result, setResult] = useState<{ data: Reconciliation; context: ReconciliationContext }>();
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  const [visible, setVisible] = useState(50);
  const [exporting, setExporting] = useState(false);
  const [exportFailure, setExportFailure] = useState('');
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setResult(undefined);
    setFailure('');
    setExportFailure('');
    setVisible(50);
    void (async () => {
      const [data, owner] = await Promise.all([
        request<Reconciliation>(
          '/api/v1/counterparty-reconciliation/get',
          session,
          {
            counterparty_id: counterpartyId,
            own_company_id: filters.own_company_id || undefined,
            contract_id: filters.contract_id || undefined,
            currency: filters.currency || undefined,
            date_from: from,
            date_to: to,
          },
          controller.signal,
        ),
        filters.own_company_id
          ? request<{ counterparty: { name: string } }>(
              '/api/v1/counterparties/get',
              session,
              { id: filters.own_company_id },
              controller.signal,
            )
          : Promise.resolve(undefined),
      ]);
      if (!controller.signal.aborted)
        setResult({
          data,
          context: {
            counterparty: counterpartyName,
            ownCompany: owner?.counterparty.name ?? 'Все собственные юрлица',
            contract: contractName || 'Все договоры',
            from: from.toDisplayString(),
            to: to.toDisplayString(),
          },
        });
    })()
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [counterpartyId, counterpartyName, filters, from, to, contractName, session, version]);
  if (loading) return <p role="status">Формируем акт сверки…</p>;
  if (failure)
    return (
      <p className="error" role="alert">
        {failure}
      </p>
    );
  if (!result) return null;
  const { data, context } = result;
  return (
    <section className="reconciliation-result" aria-label="Акт сверки">
      <div className="section-heading">
        <div>
          <h2>Акт сверки</h2>
          <p className="hint">
            {context.from} — {context.to} · Операций: {data.operations.length}
          </p>
        </div>
        <div className="reconciliation-actions">
          <button
            disabled={exporting || !data.totals.length}
            onClick={() => {
              const url = URL.createObjectURL(
                new Blob([reconciliationCsv(data, context)], { type: 'text/csv;charset=utf-8' }),
              );
              const link = document.createElement('a');
              link.href = url;
              link.download = 'Акт_сверки.csv';
              document.body.append(link);
              link.click();
              link.remove();
              setTimeout(() => URL.revokeObjectURL(url), 30000);
            }}
          >
            Скачать CSV
          </button>
          <button
            disabled={exporting || !data.totals.length}
            onClick={async () => {
              setExporting(true);
              setExportFailure('');
              try {
                const { downloadReconciliationPdf } = await import('./reconciliationPdf');
                await downloadReconciliationPdf(data, context);
              } catch (error) {
                setExportFailure((error as Error).message);
              } finally {
                setExporting(false);
              }
            }}
          >
            {exporting ? 'Формируем PDF…' : 'Скачать PDF'}
          </button>
        </div>
      </div>
      <p className="hint">
        Положительное сальдо — контрагент должен нам, отрицательное — мы должны контрагенту. Дебет
        увеличивает сальдо, кредит уменьшает.
      </p>
      <p className="hint">
        {filters.contract_id
          ? 'По договору учтены только распределённые на его счета суммы платежей.'
          : 'Платежи учтены целиком, включая нераспределённые авансы.'}{' '}
        Архивные документы исключены. Используется текущее состояние документов.
      </p>
      {exportFailure && (
        <p className="error" role="alert">
          {exportFailure}
        </p>
      )}
      {!data.totals.length ? (
        <p className="empty-state">Операций и начального остатка за выбранный период нет</p>
      ) : (
        <>
          <div className="reconciliation-table-wrap">
            <table className="reconciliation-totals">
              <thead>
                <tr>
                  <th>Валюта</th>
                  <th>Начальное сальдо</th>
                  <th>Дебет за период</th>
                  <th>Кредит за период</th>
                  <th>Конечное сальдо</th>
                </tr>
              </thead>
              <tbody>
                {data.totals.map((total) => (
                  <tr key={total.currency}>
                    <th>{total.currency}</th>
                    <td>{formatFixed(total.opening_minor, 2)}</td>
                    <td>{formatFixed(total.debit_minor, 2)}</td>
                    <td>{formatFixed(total.credit_minor, 2)}</td>
                    <td>
                      <strong>{formatFixed(total.closing_minor, 2)}</strong>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <h3>Операции за период</h3>
          {!data.operations.length ? (
            <p className="empty-state">В периоде операций нет; показаны переходящие остатки</p>
          ) : (
            <div className="reconciliation-table-wrap">
              <table className="reconciliation-operations">
                <thead>
                  <tr>
                    <th>Дата</th>
                    <th>Документ</th>
                    <th>Наше юрлицо</th>
                    <th>Валюта</th>
                    <th>Дебет</th>
                    <th>Кредит</th>
                    <th>Сальдо</th>
                  </tr>
                </thead>
                <tbody>
                  {data.operations.slice(0, visible).map((item) => (
                    <tr key={`${item.kind}:${item.id}`}>
                      <td>{CalendarDate.fromISO(item.date).toDisplayString()}</td>
                      <td>
                        <a className="link" href={reconciliationLink(item)}>
                          {reconciliationLabels[item.kind]} № {item.number}
                        </a>
                      </td>
                      <td>{item.own_company_name}</td>
                      <td>{item.currency}</td>
                      <td>{formatFixed(item.debit_minor, 2)}</td>
                      <td>{formatFixed(item.credit_minor, 2)}</td>
                      <td>{formatFixed(item.balance_minor, 2)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
          <AutoLoadMore
            enabled={visible < data.operations.length}
            loading={false}
            failed={false}
            onLoad={async () => {
              setVisible((old) => old + 50);
            }}
          />
        </>
      )}
    </section>
  );
}
