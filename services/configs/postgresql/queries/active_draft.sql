SELECT id FROM config_drafts WHERE document->>'config_name' = $1
AND document->>'status' = 'draft';
