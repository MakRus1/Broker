CREATE TABLE invoice_print_snapshots (
    organization_id UUID NOT NULL,
    invoice_id UUID NOT NULL,
    revision BIGINT NOT NULL,
    snapshot JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (organization_id, invoice_id, revision),
    FOREIGN KEY (organization_id, invoice_id) REFERENCES invoices (organization_id, id)
);
