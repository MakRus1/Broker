SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM transportations
LEFT JOIN shipment_projections AS shipment_projection
    ON shipment_projection.organization_id = transportations.organization_id
    AND shipment_projection.shipment_id = transportations.shipment_id
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
