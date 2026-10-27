WITH selected AS (
    SELECT t.*,
        CASE $6::TEXT
            WHEN 'container_number' THEN lower(COALESCE(container_number, ''))
            WHEN 'container_type' THEN COALESCE(container_type_code, '')
            WHEN 'type' THEN type
            ELSE ''
        END COLLATE "C" AS sort_key
    FROM transportations AS t
    WHERE organization_id = $1::UUID
      AND ($2::TEXT IS NULL OR lower(COALESCE(container_number, '')) COLLATE "C" LIKE lower($2))
      AND ($3::TEXT[] IS NULL OR type = ANY($3))
      AND ($4::TEXT[] IS NULL OR container_type_code = ANY($4))
      AND ($5::BOOLEAN IS NULL OR hazardous = $5)
      AND ($11::UUID IS NULL OR shipment_id = $11::UUID)
      AND ($12::UUID IS NULL OR item_id = $12::UUID)
)
SELECT ROW(id, revision, shipment_id, item_id, operation_id,
    container_type_code, container_ownership, container_number, type, hazardous
)::public.transportation_v4, sort_key
FROM selected
WHERE $9::UUID IS NULL OR
    CASE WHEN $7::BOOLEAN THEN (sort_key, id) < ($8::TEXT COLLATE "C", $9::UUID)
         ELSE (sort_key, id) > ($8::TEXT COLLATE "C", $9::UUID) END
ORDER BY
    CASE WHEN NOT $7::BOOLEAN THEN sort_key END ASC,
    CASE WHEN $7::BOOLEAN THEN sort_key END DESC,
    CASE WHEN NOT $7::BOOLEAN THEN id END ASC,
    CASE WHEN $7::BOOLEAN THEN id END DESC
LIMIT $10;
