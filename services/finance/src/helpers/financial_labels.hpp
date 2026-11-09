#pragma once
#include <boost/uuid/string_generator.hpp>
#include <finance/sql_queries.hpp>
#include <limits>
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline models::FinancialLabel Convert(const pgtypes::FinancialLabelV1& value) {
    models::FinancialLabel result;
    result.id = boost::uuids::to_string(value.Id);
    result.name = value.Name;
    result.direction = models::ParseEnumScalar(value.Direction, models::FinancialLabelDirection{});
    result.unit = models::ParseEnumScalar(value.Unit, models::FinancialLabelUnit{});
    result.currency = value.Currency;
    result.vat_mode = models::ParseEnumScalar(value.VatMode, models::VatMode{});
    result.vat_rate_basis_points = value.VatRateBasisPoints;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

}  // namespace finance::helpers
