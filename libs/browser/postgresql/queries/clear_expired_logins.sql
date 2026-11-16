DELETE FROM browser_login_attempts WHERE expires_at <= now();
