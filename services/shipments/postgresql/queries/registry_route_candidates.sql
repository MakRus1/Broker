SELECT ({key}) AS value, ({label}) AS label, shipments.organization_id
FROM shipments
CROSS JOIN LATERAL unnest(shipments.route_segments) AS leg
