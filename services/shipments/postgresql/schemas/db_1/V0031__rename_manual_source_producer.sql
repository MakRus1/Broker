-- Ручное создание доступно из любого пользовательского приложения.
UPDATE shipment_sources
SET producer = 'user'
WHERE producer = 'web' AND source_type = 'manual';
