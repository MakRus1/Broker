-- Общие подсказки не переносим: новые значения накапливаются отдельно для каждого клиента.
DROP TABLE cargo_suggestions;
CREATE TABLE customer_cargo_suggestions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    organization_id UUID NOT NULL,
    customer_id UUID NOT NULL,
    value TEXT NOT NULL CHECK (length(value) BETWEEN 1 AND 2000),
    search_value TEXT NOT NULL,
    removed BOOLEAN NOT NULL DEFAULT FALSE,
    used_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    removed_at TIMESTAMPTZ,
    UNIQUE (organization_id, customer_id, value)
);

CREATE INDEX customer_cargo_suggestions_active_expiration
    ON customer_cargo_suggestions (used_at) WHERE NOT removed;
CREATE INDEX customer_cargo_suggestions_removed_expiration
    ON customer_cargo_suggestions (removed_at) WHERE removed;
