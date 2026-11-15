-- $1 — организация.
-- $2 — код страны.
-- $3 — налоговый номер.
SELECT pg_advisory_xact_lock(
    hashtextextended($1::UUID::TEXT || ':' || $2 || ':' || $3, 0)
);
