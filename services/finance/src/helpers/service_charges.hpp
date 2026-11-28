#pragma once
#include <converters/service_charge.hpp>
#include <finance/sql_queries.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <models/service_charge_invoice_row.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline std::vector<models::ServiceCharge> ConvertServiceCharges(const pg::ResultSet& rows) {
    std::vector<models::ServiceCharge> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<models::pg::ServiceChargeRaw>())
        result.push_back(converters::Convert(row));
    return result;
}

inline std::vector<boost::uuids::uuid> ServiceChargeIds(
    const std::vector<models::ServiceCharge>& items) {
    std::vector<boost::uuids::uuid> result;
    result.reserve(items.size());
    for (const auto& charge : items)
        result.push_back(charge.id);
    return result;
}

// Строки sql::kListServiceChargeInvoices.
inline std::vector<models::ServiceChargeInvoice> ConvertServiceChargeInvoices(
    const pg::ResultSet& rows) {
    std::vector<models::ServiceChargeInvoice> result;
    for (const auto& row : rows) {
        const auto assignment = row.As<models::ServiceChargeInvoiceRow>(pg::kRowTag);
        result.push_back(models::ServiceChargeInvoice{
            .direction = models::ParseEnumScalar(assignment.Direction, models::InvoiceDirection{}),
            .invoice_date = assignment.InvoiceDate,
            .invoice_id = assignment.InvoiceId,
            .number = assignment.Number,
            .read_only = assignment.ReadOnly,
            .service_charge_id = assignment.ServiceChargeId});
    }
    return result;
}
}  // namespace finance::helpers
