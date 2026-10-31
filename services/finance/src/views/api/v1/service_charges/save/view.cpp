#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/service_charges/save/view.hpp>

namespace finance::views::api::v1::service_charges::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    ValidateServiceChargeScope(deps, context,
        {.shipment_id = request.body.shipment_id,
            .transportation_id = request.body.transportation_id});
    const auto parties = ValidateServiceChargeParties(deps, context, request.body);
    const auto& body = request.body;
    ValidateCurrency(body.currency);
    ValidateCurrency(body.invoice_currency);
    const Rate exchange_rate = body.manual_invoice_rate_millionths.has_value()
        ? Rate{.Millionths = body.manual_invoice_rate_millionths.value(),
              .Date = body.service_date,
              .Source = "manual"}
        : deps.exchange_rates.Get(body.currency, body.invoice_currency, body.service_date);
    if (body.currency == body.invoice_currency && exchange_rate.Millionths != 1000000)
        throw ApiError<400>{
            "identity_exchange_rate", "Для одинаковых валют курс должен быть равен 1"};
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Тот же порядок блокировок, что при включении услуги: услуга, затем счёт.
    if (request.body.expected_revision != 0)
        transaction.Execute(sql::kLockServiceCharges, organization,
            std::vector{boost::uuids::string_generator{}(request.body.id)});
    const auto labels =
        transaction.Execute(sql::kGetFinancialLabel, organization, request.body.label_id);
    if (labels.IsEmpty())
        throw ApiError<404>{"label_not_found", "Финансовая метка не найдена"};
    const auto value = PrepareServiceCharge(request.body,
        Convert(labels.AsSingleRow<pgtypes::FinancialLabelV1>()), parties, exchange_rate);
    const auto rows = request.body.expected_revision == 0
        ? transaction.Execute(sql::kCreateServiceCharge, organization, value)
        : transaction.Execute(
              sql::kUpdateServiceCharge, organization, value, request.body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Услуга изменена. Обновите список"};
    const auto result = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
    transaction.Execute(sql::kWriteServiceChargeHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    const auto assignment =
        transaction.Execute(sql::kServiceChargeInvoiceAssignment, organization, value.Id);
    if (!assignment.IsEmpty()) {
        platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
        const auto invoice_id = assignment.AsSingleRow<boost::uuids::uuid>();
        const auto locked =
            transaction.Execute(sql::kLockInvoices, organization, std::vector{invoice_id});
        auto invoice = locked.AsSingleRow<pgtypes::InvoiceV1>();
        if (invoice.Revision == std::numeric_limits<std::int64_t>::max())
            throw ApiError<409>{"invalid_revision", "Достигнут предел редакций счёта"};
        if (value.Archived)
            throw ApiError<409>{
                "service_charge_invoiced", "Сначала исключите услугу из счёта, затем архивируйте"};
        ValidateInvoiceServiceCharge(value, invoice);
        const auto previous =
            transaction.Execute(sql::kGetInvoiceServiceCharges, organization, invoice_id);
        invoice.NetMinor = 0;
        invoice.VatMinor = 0;
        invoice.TotalMinor = 0;
        for (const auto& line : previous.AsSetOf<pgtypes::ServiceChargeV1>()) {
            const auto& current = line.Id == value.Id ? value : line;
            invoice.NetMinor += current.InvoiceNetMinor;
            invoice.VatMinor += current.InvoiceVatMinor;
            invoice.TotalMinor += current.InvoiceTotalMinor;
        }
        const auto revision = invoice.Revision++;
        transaction.Execute(
            sql::kUpdateInvoiceServiceCharge, organization, invoice_id, value.Id, value);
        ValidateInvoiceSettlement(transaction, organization, invoice);
        transaction.Execute(sql::kUpdateInvoice, organization, invoice, revision);
        const auto details =
            LoadInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
        transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
            identity.ActorId(), userver::formats::json::ValueBuilder(details).ExtractValue());
    }
    transaction.Commit();
    return Response200{.charge = result};
}
}  // namespace finance::views::api::v1::service_charges::save
