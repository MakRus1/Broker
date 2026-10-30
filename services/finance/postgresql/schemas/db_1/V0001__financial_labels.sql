CREATE TABLE financial_labels (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    name TEXT NOT NULL,
    direction TEXT NOT NULL,
    unit TEXT NOT NULL,
    currency TEXT NOT NULL,
    vat_mode TEXT NOT NULL,
    vat_rate_basis_points INTEGER,
    note TEXT,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, id)
);

CREATE INDEX financial_labels_name_idx
    ON financial_labels (organization_id, (lower(name) COLLATE "C"), id);

CREATE TABLE financial_label_history (
    organization_id UUID NOT NULL,
    label_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, label_id, revision),
    FOREIGN KEY (organization_id, label_id) REFERENCES financial_labels (organization_id, id)
);

CREATE TYPE public.financial_label_v1 AS (
    id UUID,
    name TEXT,
    direction TEXT,
    unit TEXT,
    currency TEXT,
    vat_mode TEXT,
    vat_rate_basis_points INTEGER,
    note TEXT,
    archived BOOLEAN,
    revision BIGINT
);
