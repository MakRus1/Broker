JOIN shipment_route_segments AS shared
    ON shared.organization_id = {owner}.organization_id
   AND shared.shipment_id = {owner}.shipment_id
LEFT JOIN route_segments AS local
    ON local.organization_id = {owner}.organization_id
   AND local.transportation_id = {owner}.id
   AND local.id = shared.id
