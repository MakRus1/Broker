UPDATE config_drafts SET document = $2, updated_at = now() WHERE id = $1;
