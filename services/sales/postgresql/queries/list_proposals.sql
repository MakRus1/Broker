-- $1 — организация.
-- $2 — идентификатор входящего запроса.
SELECT document FROM sales_proposals
WHERE organization_id = $1::UUID AND inquiry_id = $2::UUID
ORDER BY id LIMIT 100;
