CREATE TABLE sales_rfqs (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    inquiry_id UUID NOT NULL,
    revision BIGINT NOT NULL CHECK (revision > 0),
    document JSONB NOT NULL,
    PRIMARY KEY (organization_id, id),
    FOREIGN KEY (organization_id, inquiry_id) REFERENCES sales_inquiries (organization_id, id)
);
CREATE INDEX sales_rfqs_inquiry ON sales_rfqs (organization_id, inquiry_id, id);
CREATE TABLE sales_rfq_history (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, id, revision),
    FOREIGN KEY (organization_id, id) REFERENCES sales_rfqs (organization_id, id)
);
