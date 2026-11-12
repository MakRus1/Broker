SELECT id
FROM transportations
WHERE organization_id = $1::UUID AND shipment_id = $2::UUID
ORDER BY id
FOR SHARE;
