UPDATE sales_inquiries AS target
SET manager_subject = value.manager_subject,
    status = value.status,
    cargo_description = value.cargo_description,
    incoterms = value.incoterms,
    carriage_terms = value.carriage_terms,
    loading = value.loading,
    delivery_city = value.delivery_city,
    delivery_country = value.delivery_country,
    delivery_address = value.delivery_address,
    ready_date = value.ready_date,
    desired_delivery_date = value.desired_delivery_date,
    note = value.note,
    route_segments = value.route_segments,
    conversion = NULL,
    conversion_error = NULL,
    revision = target.revision + 1
FROM jsonb_to_record($2::JSONB) AS value (
    id UUID,
    customer_id UUID,
    manager_subject TEXT,
    status TEXT,
    cargo_description TEXT,
    incoterms TEXT,
    carriage_terms TEXT,
    loading TEXT,
    delivery_city TEXT,
    delivery_country TEXT,
    delivery_address TEXT,
    ready_date DATE,
    desired_delivery_date DATE,
    note TEXT,
    route_segments JSONB
)
WHERE target.organization_id = $1::UUID
    AND target.id = value.id
    AND target.customer_id = value.customer_id
    AND target.revision = $3
    AND target.revision < 9223372036854775807
    AND (target.conversion IS NULL OR target.conversion_error IS NOT NULL)
RETURNING target.id::TEXT;
