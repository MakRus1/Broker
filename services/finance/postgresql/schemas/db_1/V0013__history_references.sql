-- Поиск истории связей без перебора всех редакций организации.
CREATE INDEX invoice_history_snapshot_references
    ON invoice_history USING GIN (snapshot jsonb_path_ops);
CREATE INDEX payment_history_snapshot_references
    ON payment_history USING GIN (snapshot jsonb_path_ops);
