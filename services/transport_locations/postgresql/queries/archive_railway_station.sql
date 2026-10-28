UPDATE railway_stations
SET archived = TRUE
WHERE esr_code = $1 AND NOT archived
RETURNING esr_code;
