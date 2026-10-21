-- Привязки назначаются доверенной административной операцией.
CREATE TABLE tenant_memberships (
    issuer TEXT NOT NULL,
    subject TEXT NOT NULL,
    tenant_id UUID NOT NULL,
    PRIMARY KEY (issuer, subject, tenant_id)
);
