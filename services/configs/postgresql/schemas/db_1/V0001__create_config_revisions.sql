CREATE TABLE config_revisions (
    revision bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    configs jsonb NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now()
);
