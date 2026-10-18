CREATE TABLE config_versions (
    name text PRIMARY KEY,
    revision bigint NOT NULL
);
-- Для существующих установок стартовая версия конфига берется из последнего снимка.
INSERT INTO config_versions(name, revision)
SELECT key, max(revision) FROM config_revisions CROSS JOIN LATERAL jsonb_object_keys(configs) AS key
GROUP BY key;

CREATE TABLE config_drafts (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    document jsonb NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX config_drafts_name ON config_drafts ((document->>'config_name'), id DESC);
CREATE UNIQUE INDEX config_drafts_active_name ON config_drafts ((document->>'config_name'))
WHERE document->>'status' = 'draft';
CREATE TABLE config_draft_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    draft_id bigint NOT NULL REFERENCES config_drafts(id),
    event jsonb NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX config_draft_events_draft ON config_draft_events(draft_id, id);
