-- $1 — организация.
-- $2 — идентификатор инициатора действия.
-- $3 — действие.
-- $4 — данные для журнала аудита в JSON.
INSERT INTO access_audit (organization_id, actor, action, document)
VALUES ($1::UUID, $2, $3, $4::JSONB);
