SELECT event, created_at::text FROM config_draft_events WHERE draft_id = $1 ORDER BY id;
