SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM sales_inquiries
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
