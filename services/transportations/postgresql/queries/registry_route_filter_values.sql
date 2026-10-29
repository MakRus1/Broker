SELECT ({key})::TEXT AS value, COALESCE(MIN(({label})::TEXT), '') AS label
FROM ({value_source}) AS candidate
WHERE {where}
GROUP BY {key}
ORDER BY {key}
LIMIT {limit};
