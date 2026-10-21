-- Одинаковые реквизиты сериализуются только в пределах организации и страны.
SELECT pg_advisory_xact_lock(hashtextextended($1, 0));
