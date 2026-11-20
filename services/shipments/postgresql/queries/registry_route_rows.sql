SELECT 1
FROM unnest(shipments.route_segments) AS leg
WHERE {where}
