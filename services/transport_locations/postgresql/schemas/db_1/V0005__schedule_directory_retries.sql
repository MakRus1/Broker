ALTER TABLE directory_sync ADD COLUMN failure_count INTEGER NOT NULL DEFAULT 0;
ALTER TABLE directory_sync ADD COLUMN last_attempt_at TIMESTAMPTZ;
ALTER TABLE directory_sync ADD COLUMN lease_until TIMESTAMPTZ;
-- Не отбираем ещё действующую аренду у предыдущей версии сервиса.
UPDATE directory_sync SET lease_until = next_attempt_at WHERE lease IS NOT NULL;
