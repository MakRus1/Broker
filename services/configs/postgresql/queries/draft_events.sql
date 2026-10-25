SELECT event, created_at FROM config_draft_events WHERE draft_id = $1 ORDER BY id;
