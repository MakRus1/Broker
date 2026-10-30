INSERT INTO payment_allocations (organization_id, payment_id, invoice_id, amount_minor)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4::BIGINT);
