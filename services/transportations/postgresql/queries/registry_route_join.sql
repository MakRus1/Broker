JOIN shipment_route_segments AS shared
    ON shared.organization_id = {owner}.organization_id
   AND shared.shipment_id = {owner}.shipment_id
LEFT JOIN LATERAL jsonb_array_elements({owner}.route_segments) AS local(document)
    ON (local.document ->> 'id')::UUID = shared.id
