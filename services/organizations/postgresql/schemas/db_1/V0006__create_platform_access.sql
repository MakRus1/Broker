-- Платформенные назначения независимы от членства в отдельной организации.
CREATE TABLE platform_access (
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    permissions TEXT[] NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (issuer, subject)
);
CREATE TABLE platform_access_audit (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    actor TEXT NOT NULL,
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    permissions TEXT[] NOT NULL,
    revision BIGINT NOT NULL
);

CREATE TYPE public.platform_access_v1 AS (
    permissions TEXT[],
    revision BIGINT
);
