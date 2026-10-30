#include <helpers/service_charges.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/service_charges/list/view.hpp>

namespace finance::views::api::v1::service_charges::list {
namespace {
// Значения source_group в service_charge_totals.sql.
enum class SummaryGroup : std::int32_t { Direct = 0, Transportations = 1, Combined = 2 };
}  // namespace

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& scope = request.body.scope;
    ValidateServiceChargeScope(deps, context, scope);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto rows = transaction.Execute(sql::kListServiceCharges, organization, scope.shipment_id,
        scope.transportation_id, request.body.after, request.body.include_archived,
        request.body.limit + 1);
    Response200 response;
    response.items = ConvertServiceCharges(rows);
    if (response.items.size() > static_cast<std::size_t>(request.body.limit)) {
        response.items.pop_back();
        response.next_after = response.items.back().id;
    }
    std::vector<boost::uuids::uuid> service_charge_ids;
    for (const auto& charge : response.items)
        service_charge_ids.push_back(boost::uuids::string_generator{}(charge.id));
    const auto invoices =
        transaction.Execute(sql::kListServiceChargeInvoices, organization, service_charge_ids);
    for (const auto& row : invoices) {
        const auto [service_charge_id, invoice_id, number, direction, invoice_date] =
            row.As<boost::uuids::uuid, boost::uuids::uuid, std::string, std::string,
                userver::utils::datetime::Date>();
        response.invoices.push_back(models::ServiceChargeInvoice{
            .direction = models::ParseEnumScalar(direction, models::InvoiceDirection{}),
            .invoice_date = invoice_date,
            .invoice_id = boost::uuids::to_string(invoice_id),
            .number = number,
            .service_charge_id = boost::uuids::to_string(service_charge_id)});
    }
    const auto totals = transaction.Execute(
        sql::kServiceChargeTotals, organization, scope.shipment_id, scope.transportation_id);
    for (const auto& row : totals) {
        const auto [group, currency, income, expense] =
            row.As<std::int32_t, std::string, std::string, std::string>();
        models::CurrencyTotal total{
            .currency = currency, .expense_minor = expense, .income_minor = income};
        switch (static_cast<SummaryGroup>(group)) {
        case SummaryGroup::Direct:
            response.summary.own.push_back(total);
            break;
        case SummaryGroup::Transportations:
            response.summary.transportations.push_back(total);
            break;
        case SummaryGroup::Combined:
            response.summary.combined.push_back(total);
            break;
        default:
            throw std::runtime_error("Неизвестная группа финансовой сводки");
        }
    }
    transaction.Commit();
    return response;
}
}  // namespace finance::views::api::v1::service_charges::list
