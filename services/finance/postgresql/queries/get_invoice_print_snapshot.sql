SELECT snapshot
FROM invoice_print_snapshots
WHERE organization_id = $1::UUID
    AND invoice_id = $2::UUID
    AND revision = $3::BIGINT;
