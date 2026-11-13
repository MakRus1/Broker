#pragma once
#include <models/pg/invoice_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <span>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::invoice_operations {
namespace pg = userver::storages::postgres;

// Операции используют транзакцию сценария и не начинают собственную.
void RecalculateAmounts(
    models::pg::InvoiceRaw& invoice, std::span<const models::pg::ServiceChargeRaw> charges);

void ValidateInvoiceServiceCharge(
    const models::pg::ServiceChargeRaw& charge, const models::pg::InvoiceRaw& invoice);

void ValidateInvoiceSettlement(pg::Transaction& transaction, const std::string& organization,
    const models::pg::InvoiceRaw& invoice);

models::InvoiceDetails LoadInvoice(
    pg::Transaction& transaction, const std::string& organization, const std::string& id);

void RemoveInvoiceServiceCharge(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& invoice_id, const boost::uuids::uuid& service_charge_id,
    const platform::auth::Identity& identity);

void CheckInvoiceAssignment(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& service_charge_id, const std::optional<std::string>& previous);

models::InvoiceDetails IncludeServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const platform::auth::Identity& identity,
    const models::pg::ServiceChargeRaw& charge, const std::string& invoice_id,
    std::int64_t expected_revision,
    const std::optional<std::string>& previous_invoice_id = std::nullopt);
}  // namespace finance::invoice_operations
