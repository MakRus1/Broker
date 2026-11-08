SELECT jsonb_strip_nulls(
        (to_jsonb(value) - 'conversion')
        || jsonb_build_object(
            'converting', conversion IS NOT NULL
                AND shipment_id IS NULL
                AND conversion_error IS NULL
        )
    )
FROM sales_inquiries AS value
WHERE organization_id = $1::UUID AND id = $2::UUID;
