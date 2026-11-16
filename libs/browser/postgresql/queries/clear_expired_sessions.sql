DELETE FROM browser_sessions WHERE expires_at <= now();
