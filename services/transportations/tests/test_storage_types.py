"""YAML описывает реальные атрибуты PostgreSQL-типов, включая их порядок."""

from copy import deepcopy

import pytest
import yaml

from postgres_types import assert_postgres_types


def test_storage_type_contract(pgsql, service_source_dir):
    assert_postgres_types(pgsql['db_1'], service_source_dir)


def test_contract_detects_reordered_fields(pgsql, service_source_dir):
    specification = yaml.safe_load((service_source_dir / 'postgresql/types.yaml').read_text())
    mismatched = deepcopy(specification)
    fields = mismatched['types']['public.transportation_v1']['fields']
    fields[0], fields[2] = fields[2], fields[0]
    with pytest.raises(AssertionError, match='SQL-тип и YAML расходятся'):
        assert_postgres_types(pgsql['db_1'], service_source_dir, mismatched)
