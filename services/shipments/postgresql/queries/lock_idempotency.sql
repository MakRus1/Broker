-- Одинаковый ключ сериализуется между репликами до изменения бизнес-данных.
SELECT pg_advisory_xact_lock(hashtextextended($1::TEXT || ':' || $2 || ':' || $3, 0));
