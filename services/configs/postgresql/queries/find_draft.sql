-- $1 — идентификатор черновика.
SELECT document FROM config_drafts WHERE id = $1;
