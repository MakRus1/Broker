INSERT INTO exchange_rates (requested_date, currency, rub_millionths, nominal)
SELECT $1::DATE, value.*
FROM UNNEST($2::TEXT[], $3::BIGINT[], $4::BIGINT[]) AS value(currency, rub_millionths, nominal)
ON CONFLICT (requested_date, currency) DO NOTHING;
