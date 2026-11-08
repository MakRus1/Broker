SELECT pg_advisory_xact_lock(hashtextextended('sales-settings:' || $1::TEXT, 0));
