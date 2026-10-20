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
CREATE TABLE shipments (
    tenant_id uuid NOT NULL, id uuid NOT NULL DEFAULT gen_random_uuid(),
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    number bigint GENERATED ALWAYS AS IDENTITY, customer_id uuid NOT NULL,
    reference text NOT NULL DEFAULT '',
    PRIMARY KEY (tenant_id, id), UNIQUE (tenant_id, number)
);
CREATE TABLE shipment_items (
    tenant_id uuid NOT NULL, id uuid NOT NULL DEFAULT gen_random_uuid(), shipment_id uuid NOT NULL,
    revision bigint NOT NULL DEFAULT 1 CHECK (revision > 0),
    container_type_code text NOT NULL CHECK (container_type_code ~ '^[A-Z0-9]{2,16}$'),
    container_ownership text NOT NULL CHECK (container_ownership IN ('SOC','COC')),
    planned_quantity integer NOT NULL CHECK (planned_quantity BETWEEN 1 AND 10000),
    PRIMARY KEY (tenant_id, id),
    FOREIGN KEY (tenant_id, shipment_id) REFERENCES shipments (tenant_id, id)
);
CREATE INDEX ON shipment_items (tenant_id, shipment_id, id);
CREATE TABLE creation_operations (
    tenant_id uuid NOT NULL, id uuid NOT NULL DEFAULT gen_random_uuid(), item_id uuid NOT NULL,
    quantity integer NOT NULL CHECK (quantity BETWEEN 1 AND 100),
    status text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending','succeeded','rejected')),
    payload jsonb NOT NULL, result jsonb,
    attempts integer NOT NULL DEFAULT 0, next_attempt_at timestamptz NOT NULL DEFAULT now(),
    lease_until timestamptz, lease_token uuid,
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (tenant_id,id), FOREIGN KEY (tenant_id,item_id) REFERENCES shipment_items (tenant_id,id)
);
CREATE INDEX ON creation_operations (tenant_id,item_id,status);
CREATE INDEX ON creation_operations (next_attempt_at) WHERE status='pending';
CREATE TABLE idempotent_results (
    tenant_id uuid NOT NULL, action text NOT NULL, key text NOT NULL,
    request jsonb NOT NULL, result jsonb NOT NULL,
    PRIMARY KEY (tenant_id,action,key)
);

CREATE FUNCTION shipment_json(t uuid, s uuid) RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT jsonb_build_object('id',id,'revision',revision,'number','S-'||number,
    'customer_id',customer_id,'reference',reference) FROM shipments WHERE tenant_id=t AND id=s
$$;
CREATE FUNCTION item_json(t uuid, i uuid) RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT jsonb_build_object('id',s.id,'shipment_id',s.shipment_id,'revision',s.revision,
    'container_type_code',s.container_type_code,'container_ownership',s.container_ownership,
    'planned_quantity',s.planned_quantity,'created_quantity',coalesce(o.created,0),
    'pending_quantity',coalesce(o.pending,0),'available_quantity',s.planned_quantity-coalesce(o.created,0)-coalesce(o.pending,0))
    FROM shipment_items s LEFT JOIN LATERAL (
    SELECT sum(quantity) FILTER(WHERE status='succeeded') AS created,
    sum(quantity) FILTER(WHERE status='pending') AS pending FROM creation_operations
    WHERE tenant_id=t AND item_id=i) o ON true WHERE s.tenant_id=t AND s.id=i
$$;
CREATE FUNCTION operation_json(t uuid, o uuid) RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT jsonb_build_object('id',id,'item_id',item_id,'quantity',quantity,'status',status,
    'transportation_ids',coalesce(result->'transportation_ids','[]'::jsonb)) ||
    CASE WHEN result ? 'rejection_code' THEN jsonb_build_object('rejection_code',result->'rejection_code') ELSE '{}'::jsonb END
    FROM creation_operations WHERE tenant_id=t AND id=o
$$;

-- Журнал идемпотентности блокируется до изменения бизнес-данных, в том числе для разных pod.
CREATE FUNCTION acquire_request(t uuid, a text, b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    old idempotent_results;
BEGIN
    PERFORM pg_advisory_xact_lock(hashtextextended(t::text||':'||a||':'||(b->>'idempotency_key'),0));
    SELECT * INTO old FROM idempotent_results WHERE tenant_id=t AND action=a AND key=b->>'idempotency_key';
    IF FOUND THEN
        IF old.request<>b THEN
            RETURN jsonb_build_object(
                'code',
                'idempotency_conflict',
                'message',
                'Ключ уже использован с другим запросом'
            );
        END IF;
        RETURN old.result;
    END IF;
    RETURN NULL;
END
$$;
CREATE FUNCTION save_request(t uuid,a text,b jsonb,r jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
BEGIN
    INSERT INTO idempotent_results VALUES(t,a,b->>'idempotency_key',b,r);
    RETURN r;
END
$$;
CREATE FUNCTION create_shipment(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    r jsonb;
    i uuid;
BEGIN
    r:=acquire_request(t,'create_shipment',b);
    IF r IS NOT NULL THEN
        RETURN r;
    END IF;
    INSERT INTO shipments(tenant_id,customer_id,reference) VALUES(t,(b->>'customer_id')::uuid,coalesce(b->>'reference','')) RETURNING id INTO i;
    RETURN save_request(t,'create_shipment',b,shipment_json(t,i));
END
$$;
CREATE FUNCTION get_shipment(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT shipment_json(t,(b->>'shipment_id')::uuid)
$$;
CREATE FUNCTION list_shipments(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT jsonb_build_object('items',coalesce(jsonb_agg(shipment_json(t,id) ORDER BY id),'[]'::jsonb)) FROM
    (SELECT id FROM shipments WHERE tenant_id=t AND (NOT b?'after' OR id>(b->>'after')::uuid) ORDER BY id LIMIT (b->>'limit')::int) s
$$;
CREATE FUNCTION update_shipment(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    s shipments;
BEGIN
    SELECT * INTO s FROM shipments WHERE tenant_id=t AND id=(b->>'shipment_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF s.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Поставка изменена');
    END IF;
    UPDATE shipments SET reference=b->>'reference',revision=revision+1 WHERE tenant_id=t AND id=s.id;
    RETURN shipment_json(t,s.id);
END
$$;
CREATE FUNCTION add_item(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    s shipments;
    i uuid;
    r jsonb;
BEGIN
    r:=acquire_request(t,'add_item',b);
    IF r IS NOT NULL THEN
        RETURN r;
    END IF;
    SELECT * INTO s FROM shipments WHERE tenant_id=t AND id=(b->>'shipment_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF s.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Поставка изменена');
    END IF;
    INSERT INTO shipment_items(tenant_id,shipment_id,container_type_code,container_ownership,planned_quantity)
    VALUES(t,s.id,b->>'container_type_code',b->>'container_ownership',(b->>'planned_quantity')::int) RETURNING id INTO i;
    UPDATE shipments SET revision=revision+1 WHERE tenant_id=t AND id=s.id;
    RETURN save_request(t,'add_item',b,item_json(t,i));
END
$$;
CREATE FUNCTION list_items(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT CASE WHEN EXISTS(SELECT FROM shipments WHERE tenant_id=t AND id=(b->>'shipment_id')::uuid)
    THEN (SELECT jsonb_build_object('items',coalesce(jsonb_agg(item_json(t,id) ORDER BY id),'[]'::jsonb)) FROM
    (SELECT id FROM shipment_items WHERE tenant_id=t AND shipment_id=(b->>'shipment_id')::uuid
    AND (NOT b?'after' OR id>(b->>'after')::uuid) ORDER BY id LIMIT (b->>'limit')::int) s) ELSE NULL END
$$;
CREATE FUNCTION update_item(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    i shipment_items;
    n bigint;
BEGIN
    SELECT * INTO i FROM shipment_items WHERE tenant_id=t AND id=(b->>'item_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF i.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Позиция изменена');
    END IF;
    SELECT coalesce(sum(quantity),0) INTO n FROM creation_operations WHERE tenant_id=t AND item_id=i.id AND status<>'rejected';
    IF (b->>'planned_quantity')::int<n THEN
        RETURN jsonb_build_object(
            'code',
            'quantity_reserved',
            'message',
            'Количество уже зарезервировано или создано'
        );
    END IF;
    UPDATE shipment_items SET planned_quantity=(b->>'planned_quantity')::int,revision=revision+1 WHERE tenant_id=t AND id=i.id;
    RETURN item_json(t,i.id);
END
$$;
CREATE FUNCTION create_operation(t uuid,b jsonb) RETURNS jsonb LANGUAGE plpgsql AS $$
DECLARE
    i shipment_items;
    o uuid;
    r jsonb;
    n bigint;
BEGIN
    r:=acquire_request(t,'create_operation',b);
    IF r IS NOT NULL THEN
        RETURN r;
    END IF;
    SELECT * INTO i FROM shipment_items WHERE tenant_id=t AND id=(b->>'item_id')::uuid FOR UPDATE;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF i.revision<>(b->>'expected_revision')::bigint THEN
        RETURN jsonb_build_object('code','revision_conflict','message','Позиция изменена');
    END IF;
    SELECT coalesce(sum(quantity),0) INTO n FROM creation_operations WHERE tenant_id=t AND item_id=i.id AND status<>'rejected';
    IF n+(b->>'quantity')::int>i.planned_quantity THEN
        RETURN jsonb_build_object('code','quantity_exceeded','message','Превышено плановое количество');
    END IF;
    o:=gen_random_uuid();
    r:=jsonb_build_object('operation_id',o,'tenant_id',t,'shipment_id',i.shipment_id,'item_id',i.id,
    'quantity',(b->>'quantity')::int,'schema_version',1,'container_type_code',i.container_type_code,'container_ownership',i.container_ownership);
    INSERT INTO creation_operations(tenant_id,id,item_id,quantity,payload) VALUES(t,o,i.id,(b->>'quantity')::int,r);
    RETURN save_request(t,'create_operation',b,operation_json(t,o));
END
$$;
CREATE FUNCTION get_operation(t uuid,b jsonb) RETURNS jsonb LANGUAGE sql AS $$
    SELECT operation_json(t,(b->>'operation_id')::uuid)
$$;
