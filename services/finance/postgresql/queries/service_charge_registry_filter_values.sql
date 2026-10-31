SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM service_charges AS charge
LEFT JOIN (
    SELECT line.organization_id AS invoice_organization, line.service_charge_id,
        invoice.id AS invoice_id, invoice.number AS invoice_number
    FROM invoice_service_charges AS line
    JOIN invoices AS invoice
        ON invoice.organization_id = line.organization_id AND invoice.id = line.invoice_id
    WHERE NOT line.released
) AS invoice_link
    ON invoice_link.invoice_organization = charge.organization_id
    AND invoice_link.service_charge_id = charge.id
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
