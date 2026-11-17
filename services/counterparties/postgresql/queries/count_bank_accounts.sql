-- $1 — организация.
-- $2 — идентификатор контрагента.
SELECT COUNT(*)
FROM bank_accounts
WHERE organization_id = $1::UUID AND counterparty_id = $2::UUID;
