-- $1 — дата, на которую запрошен курс.
-- $2 — коды валют.
-- $3 — курсы в миллионных долях рубля за номинал.
-- $4 — номиналы валют в том же порядке.
INSERT INTO exchange_rates (requested_date, currency, rub_millionths, nominal)
SELECT $1::DATE, value.*
FROM UNNEST($2::TEXT[], $3::BIGINT[], $4::BIGINT[]) AS value(currency, rub_millionths, nominal)
ON CONFLICT (requested_date, currency) DO NOTHING;
