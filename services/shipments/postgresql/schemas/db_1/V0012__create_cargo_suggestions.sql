CREATE TABLE cargo_suggestions (
    organization_id UUID NOT NULL,
    value TEXT NOT NULL CHECK (length(value) BETWEEN 1 AND 2000),
    search_value TEXT NOT NULL,
    removed BOOLEAN NOT NULL DEFAULT FALSE,
    used_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (organization_id, value)
);
