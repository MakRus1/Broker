-- $1 — организация.
-- $2 — идентификатор входящего запроса.
SELECT COUNT(*)
FROM sales_rfqs
WHERE organization_id = $1::UUID AND inquiry_id = $2::UUID;
