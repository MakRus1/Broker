#include <helpers/reporting.hpp>
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
    response.invoices = LoadServiceChargeInvoices(transaction, organization, response.items);
    if (!scope.shipment_id.has_value()) {
        transaction.Commit();
        const auto settings = GetFinanceSettings(deps, organization);
        response.reporting = {
            .currency = settings.reporting_currency, .missing_rates = {}, .summary = {}};
        return response;
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
    const auto reporting_rows = transaction.Execute(
        sql::kReportingServiceCharges, organization, scope.shipment_id, scope.transportation_id);
    transaction.Commit();
    const auto settings = GetFinanceSettings(deps, organization);
    response.reporting = BuildReporting(
        deps, settings.reporting_currency, reporting_rows, scope.transportation_id.has_value());
    return response;
}
}  // namespace finance::views::api::v1::service_charges::list
