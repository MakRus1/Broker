import { useLocalization } from '../../localization/Localization';
import { DecimalInput } from '../../../shared/ui/DecimalInput';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { Select } from '../../../shared/ui/Select';
import { currenciesWithCurrent } from '../currencies';
import { formatFixed, inputFixed, type ExchangeRate } from './serviceCharges';

export function ServiceExchangeFields({
  session,
  disabled,
  date,
  currency,
  invoiceCurrency,
  invoiceCurrencyLocked = false,
  manual,
  rate,
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
  invoiceCurrencyLocked?: boolean;
  manual: boolean;
  rate: string;
  onCurrency: (value: string) => void;
  onInvoiceCurrency: (value: string) => void;
  onManual: (value: boolean) => void;
  onRate: (value: string) => void;
}) {
  const { translate, locale } = useLocalization();
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
      <div className="service-exchange-fields">
        <Select
          label={translate('finance.serviceCurrency')}
          searchable
          value={currency}
          options={currenciesWithCurrent(currency, locale)}
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
          label={translate('finance.invoiceCurrency')}
          searchable
          value={invoiceCurrency}
          options={currenciesWithCurrent(invoiceCurrency, locale)}
          onChange={(value) => {
            onInvoiceCurrency(value);
            if (value === currency) {
              onManual(false);
              onRate('1');
            }
          }}
          disabled={disabled || invoiceCurrencyLocked}
        />
        <div className="service-exchange-rate">
          <span className="field-label">
            {translate('finance.ratePair', { currency, invoiceCurrency })}
          </span>
          <label className="inline-check" style={{ visibility: same ? 'hidden' : undefined }}>
            <input
              type="checkbox"
              checked={manual}
              disabled={disabled || same}
              onChange={(event) => {
                if (event.target.checked && quote) onRate(inputFixed(quote.rate_millionths, 6));
                onManual(event.target.checked);
              }}
            />
            {translate('finance.setManualRate')}
          </label>
          {!same && manual ? (
            <DecimalInput
              aria-label={translate('finance.manualRate')}
              required
              precision={6}
              value={rate}
              disabled={disabled}
              onChange={(event) => onRate(event.target.value)}
            />
          ) : (
            <output className="service-rate-value" aria-busy={!same && loading}>
              {same
                ? formatFixed(1000000, 6)
                : loading || !quote
                  ? '—'
                  : formatFixed(quote.rate_millionths, 6)}
            </output>
          )}
          <p
            className={failure && !same && !manual ? 'field-error' : 'hint'}
            role={failure && !same && !manual ? 'alert' : undefined}
          >
            {same ? (
              translate('finance.sameCurrencies')
            ) : manual ? (
              '\u00a0'
            ) : loading ? (
              translate('finance.loadingRate')
            ) : failure ? (
              <>
                {failure}{' '}
                <button type="button" onClick={() => setVersion((old) => old + 1)}>
                  {translate('filter.retry')}
                </button>
              </>
            ) : quote ? (
              translate('finance.centralBankSource', {
                date: CalendarDate.fromISO(quote.date).toDisplayString(),
              })
            ) : (
              '\u00a0'
            )}
          </p>
        </div>
      </div>
    </>
  );
}
