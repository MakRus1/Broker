CREATE TABLE transportation_legs (
    tenant_id UUID NOT NULL,
    transportation_id UUID NOT NULL,
    id UUID NOT NULL,
    position INTEGER NOT NULL,
    document JSONB NOT NULL,
    PRIMARY KEY (tenant_id, transportation_id, id),
    UNIQUE (tenant_id, transportation_id, position),
    FOREIGN KEY (tenant_id, transportation_id) REFERENCES transportations (tenant_id, id)
);
