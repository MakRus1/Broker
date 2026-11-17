-- $1 — дата, на которую запрошен курс.
-- $2 — коды валют.
SELECT rate.currency, rate.rub_millionths, rate.nominal, day.rate_date
FROM exchange_rates AS rate
JOIN exchange_rate_days AS day ON day.requested_date = rate.requested_date
WHERE rate.requested_date = $1::DATE AND rate.currency = ANY($2::TEXT[]);
