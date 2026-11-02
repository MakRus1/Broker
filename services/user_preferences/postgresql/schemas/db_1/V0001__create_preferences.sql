CREATE TABLE user_preferences (
    organization_id UUID NOT NULL,
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    key TEXT NOT NULL,
    value JSONB NOT NULL,
    revision BIGINT NOT NULL,
    PRIMARY KEY (organization_id, issuer, subject, key)
);
