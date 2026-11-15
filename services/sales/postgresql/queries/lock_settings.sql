-- $1 — организация.
SELECT pg_advisory_xact_lock(hashtextextended('sales-settings:' || $1::UUID::TEXT, 0));
