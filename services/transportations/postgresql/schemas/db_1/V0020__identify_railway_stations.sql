-- Только преобразование старых снимков. Функция не остаётся в схеме сервиса.
CREATE FUNCTION pg_temp.identify_railway_stations(value JSONB) RETURNS JSONB
LANGUAGE plpgsql AS $$
DECLARE converted JSONB;
BEGIN
    IF jsonb_typeof(value) = 'array' THEN
        SELECT coalesce(jsonb_agg(pg_temp.identify_railway_stations(element) ORDER BY position), '[]'::jsonb)
        INTO converted FROM jsonb_array_elements(value) WITH ORDINALITY AS items(element, position);
        RETURN converted;
    ELSIF jsonb_typeof(value) = 'object' THEN
        IF value ? 'esr_code' AND value ? 'name' AND value ? 'country_code' AND NOT value ? 'id' THEN
            RETURN value || jsonb_build_object(
                'id', md5('broker:railway:esr:' || (value ->> 'esr_code'))::uuid,
                'esr_source', 'manual');
        END IF;
        SELECT coalesce(jsonb_object_agg(key, pg_temp.identify_railway_stations(element)), '{}'::jsonb)
        INTO converted FROM jsonb_each(value) AS fields(key, element);
        RETURN converted;
    END IF;
    RETURN value;
END;
$$;
UPDATE route_segments SET document = pg_temp.identify_railway_stations(document);
UPDATE business_audit SET document = pg_temp.identify_railway_stations(document);
UPDATE processed_commands SET payload = pg_temp.identify_railway_stations(payload),
    result = pg_temp.identify_railway_stations(result);
DROP FUNCTION pg_temp.identify_railway_stations(JSONB);
