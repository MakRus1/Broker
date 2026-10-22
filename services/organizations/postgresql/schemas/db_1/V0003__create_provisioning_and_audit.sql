-- Намерение сохраняется до обращения к Keycloak; пароль никогда не сохраняется.
CREATE TABLE user_provisioning (
    request_id UUID PRIMARY KEY,
    organization_id UUID NOT NULL REFERENCES organizations(id),
    actor TEXT NOT NULL,
    payload JSONB NOT NULL,
    subject TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE access_audit (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    organization_id UUID NOT NULL REFERENCES organizations(id),
    actor TEXT NOT NULL,
    action TEXT NOT NULL,
    document JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
