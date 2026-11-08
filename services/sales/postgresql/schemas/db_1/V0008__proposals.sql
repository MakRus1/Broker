CREATE TABLE sales_proposals (
    organization_id UUID NOT NULL,
    inquiry_id UUID NOT NULL,
    id UUID NOT NULL,
    revision BIGINT NOT NULL CHECK (revision > 0),
    status TEXT NOT NULL CHECK (status IN ('draft', 'offered', 'accepted')),
    document JSONB NOT NULL,
    PRIMARY KEY (organization_id, id),
    FOREIGN KEY (organization_id, inquiry_id) REFERENCES sales_inquiries (organization_id, id)
);
CREATE INDEX sales_proposals_inquiry ON sales_proposals (organization_id, inquiry_id, id);
CREATE TABLE sales_proposal_history (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    revision BIGINT NOT NULL,
    actor TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    snapshot JSONB NOT NULL,
    PRIMARY KEY (organization_id, id, revision),
    FOREIGN KEY (organization_id, id) REFERENCES sales_proposals (organization_id, id)
);
ALTER TABLE sales_inquiries ADD COLUMN accepted_proposal_id UUID;
ALTER TABLE sales_inquiries ADD COLUMN accepted_proposal_revision BIGINT;
ALTER TABLE sales_inquiries ADD CONSTRAINT sales_accepted_proposal FOREIGN KEY
    (organization_id, accepted_proposal_id, accepted_proposal_revision)
    REFERENCES sales_proposal_history (organization_id, id, revision);
