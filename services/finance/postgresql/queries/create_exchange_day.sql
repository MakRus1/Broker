-- $1 — дата, на которую запрошен курс.
-- $2 — дата публикации применённого курса.
INSERT INTO exchange_rate_days (requested_date, rate_date)
VALUES ($1::DATE, $2::DATE)
ON CONFLICT (requested_date) DO NOTHING;
