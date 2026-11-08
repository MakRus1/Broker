CREATE TABLE sales_settings (
    organization_id UUID PRIMARY KEY,
    revision BIGINT NOT NULL CHECK (revision > 0),
    stages JSONB NOT NULL
);
CREATE TABLE sales_cards (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    customer_id UUID NOT NULL,
    manager_subject TEXT NOT NULL,
    stage_id UUID NOT NULL,
    source TEXT,
    note TEXT,
    next_action TEXT,
    next_action_date DATE,
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    PRIMARY KEY (organization_id, id),
    UNIQUE (organization_id, customer_id)
);
CREATE TABLE sales_inquiries (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    number BIGINT GENERATED ALWAYS AS IDENTITY,
    customer_id UUID NOT NULL,
    manager_subject TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('draft', 'in_progress', 'agreed', 'rejected')),
    cargo_description TEXT,
    incoterms TEXT,
    carriage_terms TEXT,
    loading TEXT,
    delivery_city TEXT,
    delivery_country TEXT,
    delivery_address TEXT,
    ready_date DATE,
    desired_delivery_date DATE,
    note TEXT,
    route_segments JSONB NOT NULL DEFAULT '[]',
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision > 0),
    shipment_id UUID,
    conversion JSONB,
    conversion_error TEXT,
    PRIMARY KEY (organization_id, id),
    UNIQUE (organization_id, shipment_id),
    CHECK (desired_delivery_date IS NULL OR ready_date IS NULL
        OR desired_delivery_date >= ready_date),
    CHECK (shipment_id IS NULL OR conversion IS NOT NULL)
);
CREATE INDEX sales_inquiries_customer ON sales_inquiries (organization_id, customer_id, id);
CREATE INDEX sales_cards_customer ON sales_cards (organization_id, customer_id, id);
CREATE TABLE sales_history (
    organization_id UUID NOT NULL,
    entity TEXT NOT NULL CHECK (entity IN ('cards', 'inquiries')),
    id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, entity, id, revision)
);
