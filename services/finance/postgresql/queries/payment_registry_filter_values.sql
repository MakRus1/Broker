SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM payments
{facet_join}
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
