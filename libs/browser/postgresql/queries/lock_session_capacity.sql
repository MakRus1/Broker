-- $1 — имя таблицы с ограничением размера.
SELECT pg_advisory_xact_lock(hashtextextended($1, 0));
