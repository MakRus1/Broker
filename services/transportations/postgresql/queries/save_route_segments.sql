-- $1 — организация.
-- $2 — идентификатор перевозки.
-- $3 — идентификаторы участков в порядке маршрута.
-- $4 — данные участков в том же порядке.
INSERT INTO route_segments (organization_id, transportation_id, id, position, document)
SELECT $1::UUID, $2::UUID, segment.id, (segment.ordinality - 1)::INTEGER, segment.document
FROM UNNEST($3::UUID[], $4::JSONB[]) WITH ORDINALITY AS segment(id, document, ordinality)
ON CONFLICT (organization_id, transportation_id, id)
DO UPDATE SET position = EXCLUDED.position, document = EXCLUDED.document;
