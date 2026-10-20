WITH candidate AS (
 SELECT tenant_id,id FROM creation_operations WHERE status='pending'
 AND next_attempt_at<=now() AND (lease_until IS NULL OR lease_until<now())
 ORDER BY next_attempt_at,id FOR UPDATE SKIP LOCKED LIMIT 1
), claimed AS (
 UPDATE creation_operations o SET lease_until=now()+interval '30 seconds',lease_token=gen_random_uuid(),attempts=attempts+1
 FROM candidate c WHERE o.tenant_id=c.tenant_id AND o.id=c.id
 RETURNING o.*
)
SELECT jsonb_build_object('tenant_id',tenant_id,'id',id,'lease_token',lease_token,'payload',payload) FROM claimed;
