CREATE TABLE memberships (
    organization_id UUID NOT NULL REFERENCES organizations(id),
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    username TEXT NOT NULL,
    display_name TEXT NOT NULL,
    email TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('administrator', 'operator', 'viewer')),
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    revision BIGINT NOT NULL DEFAULT 1,
    PRIMARY KEY (organization_id, issuer, subject)
);

CREATE INDEX memberships_identity ON memberships (issuer, subject, organization_id);

CREATE TYPE public.member_v1 AS (
    subject TEXT,
    username TEXT,
    display_name TEXT,
    email TEXT,
    role TEXT,
    enabled BOOLEAN,
    revision BIGINT
);
