CREATE TABLE document_number_counters (
    organization_id UUID NOT NULL,
    kind TEXT NOT NULL,
    period INTEGER NOT NULL,
    value BIGINT NOT NULL,
    PRIMARY KEY (organization_id, kind, period)
);

CREATE TABLE contract_numbering (
    organization_id UUID PRIMARY KEY,
    number_template TEXT NOT NULL,
    reset_number_yearly BOOLEAN NOT NULL,
    revision BIGINT NOT NULL
);
