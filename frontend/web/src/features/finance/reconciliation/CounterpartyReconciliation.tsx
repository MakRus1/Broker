import { useLocalization } from '../../localization/Localization';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import type { Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { formatFixed } from '../services/serviceCharges';
import type { SettlementFilters } from '../settlements/settlements';
import {
  reconciliationCsv,
  reconciliationLabelKeys,
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
  const { translate, locale } = useLocalization();
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
            ownCompany: owner?.counterparty.name ?? translate('forms.allOwnLegalEntities'),
            contract: contractName || translate('forms.allContracts'),
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
  if (loading) return <p role="status">{translate('forms.preparingReconciliationStatement')}</p>;
  if (failure)
    return (
      <p className="error" role="alert">
        {failure}
      </p>
    );
  if (!result) return null;
  const { data, context } = result;
  return (
    <section
      className="reconciliation-result"
      aria-label={translate('forms.reconciliationStatement')}
    >
      <div className="section-heading">
        <div>
          <h2>{translate('forms.reconciliationStatement')}</h2>
          <p className="hint">
            {translate('forms.reconciliationPeriod', {
              from: context.from,
              to: context.to,
              count: data.operations.length,
            })}
          </p>
        </div>
        <div className="reconciliation-actions">
          <button
            disabled={exporting || !data.totals.length}
            onClick={() => {
              const url = URL.createObjectURL(
                new Blob([reconciliationCsv(data, context, locale)], {
                  type: 'text/csv;charset=utf-8',
                }),
              );
              const link = document.createElement('a');
              link.href = url;
              link.download = translate('forms.reconciliationCsv');
              document.body.append(link);
              link.click();
              link.remove();
              setTimeout(() => URL.revokeObjectURL(url), 30000);
            }}
          >
            {translate('forms.downloadCSV')}
          </button>
          <button
            disabled={exporting || !data.totals.length}
            onClick={async () => {
              setExporting(true);
              setExportFailure('');
              try {
                const { downloadReconciliationPdf } = await import('./reconciliationPdf');
                await downloadReconciliationPdf(data, context, locale);
              } catch (error) {
                setExportFailure((error as Error).message);
              } finally {
                setExporting(false);
              }
            }}
          >
            {exporting ? translate('forms.preparingPDF') : translate('forms.downloadPDF')}
          </button>
        </div>
      </div>
      <p className="hint">{translate('settlements.balanceExplanation')}</p>
      <p className="hint">
        {filters.contract_id
          ? translate('settlements.contractPaymentsHint')
          : translate('settlements.allPaymentsHint')}{' '}
        {translate('settlements.currentDocumentsHint')}
      </p>
      {exportFailure && (
        <p className="error" role="alert">
          {exportFailure}
        </p>
      )}
      {!data.totals.length ? (
        <p className="empty-state">
          {translate('forms.noTransactionsOrOpeningBalanceForThisPeriod')}
        </p>
      ) : (
        <>
          <div className="reconciliation-table-wrap">
            <table className="reconciliation-totals">
              <thead>
                <tr>
                  <th>{translate('registryField.currency')}</th>
                  <th>{translate('forms.openingBalance')}</th>
                  <th>{translate('forms.periodDebit')}</th>
                  <th>{translate('forms.periodCredit')}</th>
                  <th>{translate('forms.closingBalance')}</th>
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
          <h3>{translate('forms.periodTransactions')}</h3>
          {!data.operations.length ? (
            <p className="empty-state">
              {translate('forms.noTransactionsInThisPeriodCarriedForwardBalancesAreShown')}
            </p>
          ) : (
            <div className="reconciliation-table-wrap">
              <table className="reconciliation-operations">
                <thead>
                  <tr>
                    <th>{translate('forms.date')}</th>
                    <th>{translate('forms.document')}</th>
                    <th>{translate('forms.ownLegalEntity')}</th>
                    <th>{translate('registryField.currency')}</th>
                    <th>{translate('forms.debit')}</th>
                    <th>{translate('forms.credit')}</th>
                    <th>{translate('forms.balance')}</th>
                  </tr>
                </thead>
                <tbody>
                  {data.operations.slice(0, visible).map((item) => (
                    <tr key={`${item.kind}:${item.id}`}>
                      <td>{CalendarDate.fromISO(item.date).toDisplayString()}</td>
                      <td>
                        <a className="link" href={reconciliationLink(item)}>
                          {translate(reconciliationLabelKeys[item.kind])} № {item.number}
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
