SELECT ROW(dictionary, code, name)::public.shipment_dictionary_value_v1
FROM shipment_dictionary_values
WHERE active
ORDER BY dictionary, position, code;
