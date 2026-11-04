CREATE TABLE localization_catalogs (
    locale TEXT PRIMARY KEY CHECK (locale IN ('ru', 'en')),
    messages JSONB NOT NULL CHECK (jsonb_typeof(messages) = 'object' AND octet_length(messages::TEXT) <= 2097152),
    revision BIGINT NOT NULL CHECK (revision > 0),
    published_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE organization_localization_catalogs (
    organization_id UUID NOT NULL,
    locale TEXT NOT NULL CHECK (locale IN ('ru', 'en')),
    messages JSONB NOT NULL CHECK (jsonb_typeof(messages) = 'object' AND octet_length(messages::TEXT) <= 2097152),
    revision BIGINT NOT NULL CHECK (revision > 0),
    published_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (organization_id, locale)
);
