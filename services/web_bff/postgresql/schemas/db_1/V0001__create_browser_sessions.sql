CREATE TABLE browser_login_attempts (
    id TEXT PRIMARY KEY,
    verifier TEXT NOT NULL,
    nonce TEXT NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL
);

CREATE INDEX browser_login_attempts_expires_at_idx ON browser_login_attempts (expires_at);

CREATE TABLE browser_sessions (
    id TEXT PRIMARY KEY,
    actor TEXT NOT NULL,
    access_token TEXT NOT NULL,
    refresh_token TEXT NOT NULL,
    csrf TEXT NOT NULL,
    token_expires_at TIMESTAMPTZ NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL
);

CREATE INDEX browser_sessions_expires_at_idx ON browser_sessions (expires_at);
