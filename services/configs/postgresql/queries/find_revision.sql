-- $1 — номер опубликованной редакции снимка конфигов.
SELECT revision, configs, created_at FROM config_revisions WHERE revision = $1;
