SELECT currency, rub_millionths, nominal
FROM exchange_rates
WHERE requested_date = $1::DATE AND currency = ANY($2::TEXT[]);
