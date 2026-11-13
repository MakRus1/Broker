#pragma once
#include <boost/uuid/string_generator.hpp>
#include <models/financial_label_registry_row.hpp>
#include <models/pg/financial_label_raw.hpp>
#include <openapi/models.hpp>

namespace finance::converters {

inline models::FinancialLabel Convert(const models::pg::FinancialLabelRaw& value) {
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

inline std::vector<models::FinancialLabel> Convert(
    const std::vector<models::FinancialLabelRegistryRow>& rows) {
    std::vector<models::FinancialLabel> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Label));
    return result;
}
}  // namespace finance::converters
