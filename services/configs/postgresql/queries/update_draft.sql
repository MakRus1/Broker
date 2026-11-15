-- $1 — идентификатор черновика.
-- $2 — новое содержимое черновика в JSON.
UPDATE config_drafts SET document = $2, updated_at = now() WHERE id = $1;
