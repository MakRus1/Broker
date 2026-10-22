SELECT group_name, code, name, requires FROM permission_catalog
WHERE scope = 'platform' ORDER BY group_name, code;
