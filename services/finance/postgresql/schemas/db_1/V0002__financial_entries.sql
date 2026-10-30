CREATE TABLE financial_entries (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    shipment_id UUID NOT NULL,
    transportation_id UUID,
    label_id UUID NOT NULL,
    label_name TEXT NOT NULL,
    direction TEXT NOT NULL,
    unit TEXT NOT NULL,
    counterparty_id UUID NOT NULL,
    counterparty_name TEXT NOT NULL,
    contract_id UUID,
    contract_number TEXT,
    quantity_milli BIGINT NOT NULL,
    price_minor BIGINT NOT NULL,
    currency TEXT NOT NULL,
    vat_mode TEXT NOT NULL,
    vat_rate_basis_points INTEGER,
    net_minor BIGINT NOT NULL,
    vat_minor BIGINT NOT NULL,
    total_minor BIGINT NOT NULL,
    note TEXT,
    archived BOOLEAN NOT NULL,
    revision BIGINT NOT NULL,
    price_mode TEXT NOT NULL,
    PRIMARY KEY (organization_id, id)
);

CREATE INDEX financial_entries_scope ON financial_entries (organization_id, shipment_id, transportation_id, id);

CREATE TABLE financial_entry_history (
    organization_id UUID NOT NULL,
    entry_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, entry_id, revision),
    FOREIGN KEY (organization_id, entry_id) REFERENCES financial_entries (organization_id, id)
);

CREATE TYPE public.financial_entry_v1 AS (
    id UUID,
    shipment_id UUID,
    transportation_id UUID,
    label_id UUID,
    label_name TEXT,
    direction TEXT,
    unit TEXT,
    counterparty_id UUID,
    counterparty_name TEXT,
    contract_id UUID,
    contract_number TEXT,
    quantity_milli BIGINT,
    price_minor BIGINT,
    currency TEXT,
    vat_mode TEXT,
    vat_rate_basis_points INTEGER,
    net_minor BIGINT,
    vat_minor BIGINT,
    total_minor BIGINT,
    note TEXT,
    archived BOOLEAN,
    revision BIGINT,
    price_mode TEXT
);
