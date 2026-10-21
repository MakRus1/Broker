-- UUID нормализуется до вычисления ключа блокировки.
SELECT pg_advisory_xact_lock(hashtextextended($1::UUID::TEXT || ':' || $2::UUID::TEXT, 0));
