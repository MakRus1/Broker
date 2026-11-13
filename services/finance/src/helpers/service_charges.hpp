#pragma once
#include <converters/financial_label.hpp>
#include <converters/invoice.hpp>
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

inline std::vector<models::ServiceChargeInvoice> LoadServiceChargeInvoices(
    pg::Transaction& transaction, const std::string& organization,
    const std::vector<models::ServiceCharge>& items) {
    std::vector<models::ServiceChargeInvoice> result;
    std::vector<boost::uuids::uuid> service_charge_ids;
    for (const auto& charge : items)
        service_charge_ids.push_back(boost::uuids::string_generator{}(charge.id));
    const auto invoices =
        transaction.Execute(sql::kListServiceChargeInvoices, organization, service_charge_ids);
    for (const auto& row : invoices) {
        const auto assignment = row.As<models::ServiceChargeInvoiceRow>(pg::kRowTag);
        result.push_back(models::ServiceChargeInvoice{
            .direction = models::ParseEnumScalar(assignment.Direction, models::InvoiceDirection{}),
            .invoice_date = assignment.InvoiceDate,
            .invoice_id = boost::uuids::to_string(assignment.InvoiceId),
            .number = assignment.Number,
            .read_only = assignment.ReadOnly,
            .service_charge_id = boost::uuids::to_string(assignment.ServiceChargeId)});
    }
    return result;
}
}  // namespace finance::helpers
