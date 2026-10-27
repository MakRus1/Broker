SELECT
    ROW(
        id,
        name,
        revision,
        archived,
        legal_name,
        country_code,
        tax_id,
        registration_id,
        legal_address,
        roles,
        contacts,
        kpp,
        logist
    )::public.counterparty_v5,
    {sort_key} AS sort_key
FROM counterparties
WHERE {where}
ORDER BY {order_by}
LIMIT {limit};
