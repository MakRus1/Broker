-- $1 — темы, для которых нужна статистика очереди.
SELECT topic, count(*)::BIGINT,
       extract(epoch FROM min(created_at))::DOUBLE PRECISION
FROM outbox
WHERE topic = ANY($1::TEXT[])
GROUP BY topic;
