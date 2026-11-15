-- $1 — идентификатор черновика.
-- $2 — данные события черновика в JSON.
INSERT INTO config_draft_events(draft_id, event) VALUES ($1, $2);
