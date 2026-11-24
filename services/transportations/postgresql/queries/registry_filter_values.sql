SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM transportations
LEFT JOIN shipment_customer_projections AS shipment_customer
    ON shipment_customer.organization_id = transportations.organization_id
    AND shipment_customer.shipment_id = transportations.shipment_id
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
