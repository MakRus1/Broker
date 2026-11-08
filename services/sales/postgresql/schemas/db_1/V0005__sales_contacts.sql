CREATE TABLE sales_contacts (
    organization_id UUID NOT NULL,
    id UUID NOT NULL,
    card_id UUID NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('call', 'email', 'meeting', 'other')),
    contact_date DATE NOT NULL,
    result TEXT NOT NULL,
    author_subject TEXT NOT NULL,
    author_issuer TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    next_action TEXT,
    next_action_date DATE,
    request JSONB NOT NULL,
    PRIMARY KEY (organization_id, id),
    FOREIGN KEY (organization_id, card_id) REFERENCES sales_cards (organization_id, id)
);
CREATE INDEX sales_contacts_card_history
    ON sales_contacts (organization_id, card_id, contact_date DESC, created_at DESC, id DESC);
