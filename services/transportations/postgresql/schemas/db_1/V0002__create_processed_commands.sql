-- Результат и созданные перевозки записываются в одной транзакции.
CREATE TABLE processed_commands (
    tenant_id UUID NOT NULL,
    operation_id UUID NOT NULL,
    payload JSONB NOT NULL,
    result JSONB NOT NULL,
    PRIMARY KEY (tenant_id, operation_id)
);
