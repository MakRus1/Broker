import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import { CalendarDate } from './CalendarDate';
import { DateInput } from './DateInput';
import { Select } from './Select';
import { currenciesWithCurrent } from './currencies';
import { formatFixed, inputFixed, type ExchangeRate } from './serviceCharges';

export function ServiceExchangeFields({
  session,
  disabled,
  date,
  currency,
  invoiceCurrency,
  manual,
  rate,
  onDate,
  onCurrency,
  onInvoiceCurrency,
  onManual,
  onRate,
}: {
  session: Session;
  disabled: boolean;
  date: CalendarDate | null;
  currency: string;
  invoiceCurrency: string;
  manual: boolean;
  rate: string;
  onDate: (value: CalendarDate | null) => void;
  onCurrency: (value: string) => void;
  onInvoiceCurrency: (value: string) => void;
  onManual: (value: boolean) => void;
  onRate: (value: string) => void;
}) {
  const [quote, setQuote] = useState<ExchangeRate>();
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(false);
  const [version, setVersion] = useState(0);
  const same = currency === invoiceCurrency;
  useEffect(() => {
    if (manual || same || !date) {
      setQuote(undefined);
      setFailure('');
      setLoading(false);
      return;
    }
    const abort = new AbortController();
    setLoading(true);
    setQuote(undefined);
    setFailure('');
    request<ExchangeRate>(
      '/api/v1/exchange-rates/get',
      session,
      { from_currency: currency, to_currency: invoiceCurrency, date },
      abort.signal,
    )
      .then((value) => {
        if (!abort.signal.aborted) setQuote(value);
      })
      .catch((error) => {
        if (!abort.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!abort.signal.aborted) setLoading(false);
      });
    return () => abort.abort();
  }, [session, date, currency, invoiceCurrency, manual, same, version]);
  return (
    <>
      <label>
        Дата услуги · обязательно
        <DateInput required value={date} onChange={onDate} disabled={disabled} />
      </label>
      <Select
        label="Валюта услуги"
        searchable
        value={currency}
        options={currenciesWithCurrent(currency)}
        onChange={(value) => {
          onCurrency(value);
          if (value === invoiceCurrency) {
            onManual(false);
            onRate('1');
          }
        }}
        disabled={disabled}
      />
      <Select
        label="Валюта счёта"
        searchable
        value={invoiceCurrency}
        options={currenciesWithCurrent(invoiceCurrency)}
        onChange={(value) => {
          onInvoiceCurrency(value);
          if (value === currency) {
            onManual(false);
            onRate('1');
          }
        }}
        disabled={disabled}
      />
      <div className="service-exchange-rate">
        <span className="field-label">
          Курс: 1 {currency} в {invoiceCurrency}
        </span>
        {same ? (
          <p className="hint">1 · валюты совпадают</p>
        ) : (
          <>
            <label className="inline-check">
              <input
                type="checkbox"
                checked={manual}
                disabled={disabled}
                onChange={(event) => {
                  if (event.target.checked && quote) onRate(inputFixed(quote.rate_millionths, 6));
                  onManual(event.target.checked);
                }}
              />
              Указать вручную
            </label>
            {manual ? (
              <input
                aria-label="Курс вручную"
                required
                inputMode="decimal"
                value={rate}
                disabled={disabled}
                onChange={(event) => onRate(event.target.value)}
              />
            ) : loading ? (
              <p className="hint">Получаем курс ЦБ…</p>
            ) : quote ? (
              <p className="hint">
                {formatFixed(quote.rate_millionths, 6)} · ЦБ на{' '}
                {CalendarDate.fromISO(quote.date).toDisplayString()}
              </p>
            ) : failure ? (
              <p className="field-error" role="alert">
                {failure}{' '}
                <button type="button" onClick={() => setVersion((old) => old + 1)}>
                  Повторить
                </button>
              </p>
            ) : null}
          </>
        )}
      </div>
    </>
  );
}
