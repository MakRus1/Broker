-- $1 — дата, на которую запрошен курс.
SELECT rate_date FROM exchange_rate_days WHERE requested_date = $1::DATE;
