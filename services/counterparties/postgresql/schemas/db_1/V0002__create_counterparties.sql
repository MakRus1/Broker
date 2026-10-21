CREATE TABLE counterparties (
    tenant_id UUID NOT NULL,
    id UUID NOT NULL,
    name TEXT NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 256),
    PRIMARY KEY (tenant_id, id)
);

CREATE TYPE public.counterparty_v1 AS (
    id UUID,
    name TEXT
);
