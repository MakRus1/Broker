CREATE TABLE organizations (
    id UUID PRIMARY KEY,
    name TEXT NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 200),
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    revision BIGINT NOT NULL DEFAULT 1
);

CREATE TYPE public.organization_v1 AS (
    id UUID,
    name TEXT,
    enabled BOOLEAN,
    revision BIGINT
);
