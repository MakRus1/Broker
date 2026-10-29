SELECT 1
FROM transportations AS route_owner
{route_join}
WHERE route_owner.organization_id = transportations.organization_id
  AND route_owner.id = transportations.id
  AND ({where})
