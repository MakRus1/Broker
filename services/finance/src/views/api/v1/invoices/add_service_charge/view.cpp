#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/add_service_charge/view.hpp>

namespace finance::views::api::v1::invoices::add_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.body.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<pgtypes::ServiceChargeV1>();
    CheckInvoiceAssignment(transaction, organization, charge.Id, request.body.previous_invoice_id);
    std::vector<boost::uuids::uuid> invoice_ids{
        boost::uuids::string_generator{}(request.body.invoice_id)};
    if (request.body.previous_invoice_id.has_value())
        invoice_ids.push_back(
            boost::uuids::string_generator{}(request.body.previous_invoice_id.value()));
    transaction.Execute(sql::kLockInvoices, organization, invoice_ids);
    const auto invoices =
        transaction.Execute(sql::kGetInvoice, organization, request.body.invoice_id);
    if (invoices.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    auto value = invoices.AsSingleRow<pgtypes::InvoiceV1>();
    if (value.Archived)
        throw ApiError<409>{"archived_invoice", "Выберите действующий счёт"};
    if (value.Revision != request.body.expected_revision ||
        value.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите список"};
    if (transaction.Execute(sql::kGetInvoiceServiceCharges, organization, value.Id).Size() >= 500)
        throw ApiError<400>{
            "too_many_service_charges", "В одном счёте может быть не больше 500 услуг"};
    ValidateInvoiceServiceCharge(charge, value);
    if (request.body.previous_invoice_id.has_value()) {
        if (SameId(request.body.previous_invoice_id.value(), request.body.invoice_id))
            throw ApiError<400>{"same_invoice", "Выберите другой счёт"};
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(request.body.previous_invoice_id.value()), charge.Id,
            identity.ActorId());
    }
    value.NetMinor += charge.NetMinor;
    value.VatMinor += charge.VatMinor;
    value.TotalMinor += charge.TotalMinor;
    ++value.Revision;
    transaction.Execute(sql::kUpdateInvoice, organization, value, request.body.expected_revision);
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, std::vector{charge}, false);
    const auto result = LoadInvoice(transaction, organization, request.body.invoice_id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return Response200{.charges = result.charges, .invoice = result.invoice};
}
}  // namespace finance::views::api::v1::invoices::add_service_charge
