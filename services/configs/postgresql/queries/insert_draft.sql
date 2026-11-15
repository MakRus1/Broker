-- $1 — содержимое черновика в JSON.
INSERT INTO config_drafts(document) VALUES ($1) RETURNING id;
