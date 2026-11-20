SELECT 1
FROM shipments
CROSS JOIN LATERAL unnest(shipments.route_segments) AS leg
WHERE shipments.organization_id = candidate.organization_id
  AND ({key}) = candidate.value
  AND ({where})
