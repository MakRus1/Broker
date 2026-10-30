DELETE FROM payment_allocations
WHERE organization_id = $1::UUID AND payment_id = $2::UUID;
