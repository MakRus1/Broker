-- $1 — организация.
-- $2 — идентификатор счёта.
-- HTTP-запросы реквизитов завершены до этой короткой блокировки.
SELECT revision
FROM invoices
WHERE organization_id = $1::UUID AND id = $2::UUID
FOR SHARE;
