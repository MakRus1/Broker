SELECT jsonb_strip_nulls(
        (to_jsonb(value) - 'organization_id' - 'conversion' - 'number')
        || jsonb_build_object(
            'number', value.number,
            'converting', value.conversion IS NOT NULL
                AND value.shipment_id IS NULL
                AND value.conversion_error IS NULL
        )
    )
FROM sales_inquiries AS value
WHERE organization_id = $1::UUID AND id = $2::UUID;
