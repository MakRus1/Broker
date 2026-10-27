WITH selected AS (
    SELECT
        shipment.*,
        CASE $8::TEXT
            WHEN 'number' THEN lower(number)
            WHEN 'status' THEN lower(status)
            WHEN 'cargo_description' THEN lower(COALESCE(cargo_description, ''))
            ELSE ''
        END COLLATE "C" AS text_key,
        CASE
            WHEN $8::TEXT = 'shipment_date' THEN shipment_date
        END AS date_key
    FROM shipments AS shipment
    WHERE organization_id = $1::UUID
      AND (
          $2::TEXT IS NULL
          OR lower(number) COLLATE "C" LIKE lower($2)
      )
      AND (
          $3::TEXT IS NULL
          OR lower(COALESCE(cargo_description, '')) COLLATE "C" LIKE lower($3)
      )
      AND (
          $4::UUID IS NULL
          OR customer_id = $4::UUID
      )
      AND (
          $5::TEXT[] IS NULL
          OR status = ANY($5)
      )
      AND (
          $6::DATE IS NULL
          OR shipment_date >= $6::DATE
      )
      AND (
          $7::DATE IS NULL
          OR shipment_date <= $7::DATE
      )
)
SELECT
    ROW(
        id,
        revision,
        number,
        customer_id,
        reference,
        shipment_date,
        recipient_id,
        logist,
        cargo_description,
        status,
        incoterms,
        carriage_terms,
        loading,
        delivery_city,
        delivery_country,
        delivery_address,
        route_segments
    )::public.shipment_v4,
    text_key,
    date_key
FROM selected
WHERE $12::UUID IS NULL
   OR CASE
       WHEN $8::TEXT = 'shipment_date' THEN
           CASE
               WHEN $9::BOOLEAN THEN (date_key, id) < ($11::DATE, $12::UUID)
               ELSE (date_key, id) > ($11::DATE, $12::UUID)
           END
       ELSE
           CASE
               WHEN $9::BOOLEAN THEN (text_key, id) < ($10::TEXT COLLATE "C", $12::UUID)
               ELSE (text_key, id) > ($10::TEXT COLLATE "C", $12::UUID)
           END
   END
ORDER BY
    CASE WHEN NOT $9::BOOLEAN THEN date_key END ASC,
    CASE WHEN $9::BOOLEAN THEN date_key END DESC,
    CASE WHEN NOT $9::BOOLEAN THEN text_key END ASC,
    CASE WHEN $9::BOOLEAN THEN text_key END DESC,
    CASE WHEN NOT $9::BOOLEAN THEN id END ASC,
    CASE WHEN $9::BOOLEAN THEN id END DESC
LIMIT $13;
