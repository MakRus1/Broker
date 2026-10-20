-- Привязки создаются администратором; публичного назначения tenant нет.
CREATE TABLE tenant_memberships (
    issuer text NOT NULL, subject text NOT NULL, tenant_id uuid NOT NULL,
    PRIMARY KEY (issuer, subject, tenant_id)
);
CREATE TABLE business_audit (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    tenant_id uuid NOT NULL, actor text NOT NULL, action text NOT NULL,
    entity_id uuid NOT NULL, document jsonb NOT NULL, occurred_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX ON business_audit (tenant_id, entity_id, id);
CREATE TABLE processed_commands (
 tenant_id uuid NOT NULL, operation_id uuid NOT NULL, payload jsonb NOT NULL, result jsonb NOT NULL,
 PRIMARY KEY(tenant_id,operation_id)
);
CREATE TABLE transportations (
 tenant_id uuid NOT NULL, id uuid NOT NULL DEFAULT gen_random_uuid(),
 shipment_id uuid NOT NULL, item_id uuid NOT NULL, operation_id uuid NOT NULL, ordinal integer NOT NULL,
 revision bigint NOT NULL DEFAULT 1, container_type_code text NOT NULL,
 container_ownership text NOT NULL CHECK(container_ownership IN ('SOC','COC')),
 PRIMARY KEY(tenant_id,id), UNIQUE(tenant_id,operation_id,ordinal),
 FOREIGN KEY(tenant_id,operation_id) REFERENCES processed_commands(tenant_id,operation_id)
);
CREATE INDEX ON transportations(tenant_id,shipment_id,id);
CREATE INDEX ON transportations(tenant_id,item_id,id);
CREATE TABLE transportation_legs (
 tenant_id uuid NOT NULL, transportation_id uuid NOT NULL, id uuid NOT NULL, position integer NOT NULL,
 document jsonb NOT NULL,
 PRIMARY KEY(tenant_id,transportation_id,id), UNIQUE(tenant_id,transportation_id,position),
 FOREIGN KEY(tenant_id,transportation_id) REFERENCES transportations(tenant_id,id)
);
CREATE FUNCTION transportation_json(t uuid,i uuid) RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT jsonb_build_object('id',s.id,'revision',s.revision,'shipment_id',s.shipment_id,'item_id',s.item_id,
    'operation_id',s.operation_id,'container_type_code',s.container_type_code,'container_ownership',s.container_ownership,
    'legs',coalesce((SELECT jsonb_agg(document ORDER BY position) FROM transportation_legs WHERE tenant_id=t AND transportation_id=i),'[]'::jsonb))
    FROM transportations s WHERE s.tenant_id=t AND s.id=i
$$;
CREATE FUNCTION ensure_created(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    old processed_commands;
    ids jsonb:='[]';
    i uuid;
    r jsonb;
    n integer;
BEGIN
    IF (b->>'tenant_id')::uuid<>t THEN
        RETURN jsonb_build_object('code','forbidden','message','Нет доступа к клиенту');
    END IF;
    PERFORM pg_advisory_xact_lock(hashtextextended(t::text||':'||(b->>'operation_id'),0));
    SELECT * INTO old FROM processed_commands WHERE tenant_id=t AND operation_id=(b->>'operation_id')::uuid;
    IF FOUND THEN
        IF old.payload<>b THEN
            RETURN jsonb_build_object('code','protocol_conflict','message','Содержимое операции изменено');
        END IF;
        RETURN old.result;
    END IF;
    r:=jsonb_build_object(
        'id',
        b->'operation_id',
        'item_id',
        b->'item_id',
        'quantity',
        b->'quantity',
        'status',
        'succeeded',
        'transportation_ids',
        '[]'::jsonb
    );
    INSERT INTO processed_commands VALUES(t,(b->>'operation_id')::uuid,b,r);
    FOR n IN 1..(b->>'quantity')::int LOOP
        INSERT INTO transportations(tenant_id,shipment_id,item_id,operation_id,ordinal,container_type_code,container_ownership)
        VALUES(t,(b->>'shipment_id')::uuid,(b->>'item_id')::uuid,(b->>'operation_id')::uuid,n,b->>'container_type_code',b->>'container_ownership') RETURNING id INTO i;
        ids:=ids||to_jsonb(i);
    END LOOP;
    r:=jsonb_set(r,'{transportation_ids}',ids);
    UPDATE processed_commands SET result=r WHERE tenant_id=t AND operation_id=(b->>'operation_id')::uuid;
    RETURN r;
END
$$;
CREATE FUNCTION get_transportation(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT transportation_json(t,(b->>'transportation_id')::uuid)
$$;
CREATE FUNCTION list_transportations(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT jsonb_build_object(
        'items',
        coalesce(jsonb_agg(transportation_json(t,id) ORDER BY id),'[]'::jsonb)
    ) FROM
    (SELECT id FROM transportations WHERE tenant_id=t AND (NOT b?'after' OR id>(b->>'after')::uuid)
    AND (NOT b?'shipment_id' OR shipment_id=(b->>'shipment_id')::uuid)
    AND (NOT b?'item_id' OR item_id=(b->>'item_id')::uuid) ORDER BY id LIMIT (b->>'limit')::int) s
$$;
CREATE FUNCTION set_route(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    s transportations;
    leg jsonb;
    pos integer:=0;
BEGIN
    SELECT * INTO s FROM transportations WHERE tenant_id=t AND id=(b->>'transportation_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF s.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Перевозка изменена');
    END IF;
    IF EXISTS(SELECT FROM transportation_legs WHERE tenant_id=t AND transportation_id=s.id AND (document?'actual_departure_at' OR document?'actual_arrival_at'))
    THEN
        RETURN jsonb_build_object('code','route_has_actuals','message','Маршрут с фактами не заменяется');
    END IF;
    IF (SELECT count(DISTINCT (v->>'id')::uuid) FROM jsonb_array_elements(b->'legs') v)<>jsonb_array_length(b->'legs')
    THEN
        RETURN jsonb_build_object('code','invalid_request','message','ID плеч должны отличаться');
    END IF;
    FOR leg IN SELECT * FROM jsonb_array_elements(b->'legs') LOOP
        IF leg?'planned_departure_at' AND leg?'planned_arrival_at' AND (leg->>'planned_departure_at')::timestamptz>(leg->>'planned_arrival_at')::timestamptz
        THEN
            RETURN jsonb_build_object('code','invalid_request','message','Отправление позже прибытия');
        END IF;
    END LOOP;
    DELETE FROM transportation_legs WHERE tenant_id=t AND transportation_id=s.id;
    FOR leg IN SELECT * FROM jsonb_array_elements(b->'legs') LOOP
        INSERT INTO transportation_legs VALUES(t,s.id,(leg->>'id')::uuid,pos,leg);
        pos:=pos+1;
    END LOOP;
    UPDATE transportations SET revision=revision+1 WHERE tenant_id=t AND id=s.id;
    RETURN transportation_json(t,s.id);
END
$$;
CREATE FUNCTION change_actual(t uuid,b jsonb,correction boolean) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    s transportations;
    d jsonb;
    k text;
    updated jsonb;
BEGIN
    SELECT * INTO s FROM transportations WHERE tenant_id=t AND id=(b->>'transportation_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF s.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Перевозка изменена');
    END IF;
    SELECT document INTO d FROM transportation_legs WHERE tenant_id=t AND transportation_id=s.id AND id=(b->>'leg_id')::uuid;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    k:=CASE WHEN b->>'event_kind'='departure' THEN 'actual_departure_at' ELSE 'actual_arrival_at' END;
    IF (d?k)<>correction THEN
        RETURN jsonb_build_object(
            'code',
            'actual_conflict',
            'message',
            'Для исправления используйте отдельную команду'
        );
    END IF;
    updated:=jsonb_set(d,ARRAY[k],b->'occurred_at');
    IF updated?'actual_departure_at' AND updated?'actual_arrival_at' AND (updated->>'actual_departure_at')::timestamptz>(updated->>'actual_arrival_at')::timestamptz
    THEN
        RETURN jsonb_build_object('code','invalid_request','message','Отправление позже прибытия');
    END IF;
    UPDATE transportation_legs SET document=updated WHERE tenant_id=t AND transportation_id=s.id AND id=(b->>'leg_id')::uuid;
    UPDATE transportations SET revision=revision+1 WHERE tenant_id=t AND id=s.id;
    RETURN transportation_json(t,s.id);
END
$$;
CREATE FUNCTION record_actual(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT change_actual(t,b,false)
$$;
CREATE FUNCTION correct_actual(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT change_actual(t,b,true)
$$;
