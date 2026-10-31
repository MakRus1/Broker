#include <helpers/finance_settings.hpp>
#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/save/view.hpp>

namespace finance::views::api::v1::invoices::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto value = PrepareInvoice(deps, context, request.body);
    const auto ids = InvoiceServiceChargeIds(request.body);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (value.Number.empty())
        value.Number = GenerateInvoiceNumber(transaction, organization, value);
    // Общий порядок блокировок защищает от одновременного редактирования и включения услуг в
    // счета.
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization, ids);
    if (charges.Size() != ids.size())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена в организации"};
    if (request.body.previous_invoice_id.has_value()) {
        if (request.body.expected_revision != 0 || ids.size() != 1)
            throw ApiError<400>{
                "invalid_reassignment", "Новый счёт при перепривязке создаётся из одной услуги"};
        CheckInvoiceAssignment(
            transaction, organization, ids.front(), request.body.previous_invoice_id);
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(request.body.previous_invoice_id.value()), ids.front(),
            identity.ActorId());
    }
    if (!transaction.Execute(sql::kConflictingInvoiceServiceCharges, organization, ids, value.Id)
             .IsEmpty())
        throw ApiError<409>{"service_charge_invoiced", "Услуга уже включена в другой счёт"};
    std::vector<pgtypes::ServiceChargeV1> snapshots;
    snapshots.reserve(charges.Size());
    for (const auto& charge : charges.AsSetOf<pgtypes::ServiceChargeV1>()) {
        ValidateInvoiceServiceCharge(charge, value);
        snapshots.push_back(charge);
        value.NetMinor += charge.InvoiceNetMinor;
        value.VatMinor += charge.InvoiceVatMinor;
        value.TotalMinor += charge.InvoiceTotalMinor;
    }
    if (request.body.expected_revision != 0)
        transaction.Execute(sql::kLockInvoices, organization, std::vector{value.Id});
    ValidateInvoiceSettlement(transaction, organization, value);
    const auto saved = request.body.expected_revision == 0
        ? transaction.Execute(sql::kCreateInvoice, organization, value)
        : transaction.Execute(
              sql::kUpdateInvoice, organization, value, request.body.expected_revision);
    if (saved.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите карточку"};
    transaction.Execute(sql::kDeleteInvoiceServiceCharges, organization, value.Id,
        std::optional<boost::uuids::uuid>{});
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, snapshots, value.Archived);
    const auto result = LoadInvoice(transaction, organization, request.body.id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return Response200{.charges = result.charges, .invoice = result.invoice};
}
}  // namespace finance::views::api::v1::invoices::save
