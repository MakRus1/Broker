#include <helpers/financial_labels.hpp>
#include <permissions/permissions.hpp>
#include <platform/date_validation.hpp>
#include <views/api/v1/counterparty_reconciliation/get/view.hpp>

namespace finance::views::api::v1::counterparty_reconciliation::get {
namespace {
using Date = userver::utils::datetime::Date;

struct Row {
    bool IsTotal;
    std::string Currency;
    std::optional<boost::uuids::uuid> DocumentId;
    std::optional<Date> DocumentDate;
    std::optional<std::string> Kind;
    std::string Number;
    std::string OwnCompanyName;
    std::string DebitMinor;
    std::string CreditMinor;
    std::string BalanceMinor;
    std::string OpeningMinor;
};
}  // namespace

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& body = request.body;
    for (const auto date : {body.date_from, body.date_to})
        if (!platform::IsCalendarDateInRange(
                date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (body.date_from.GetSysDays() > body.date_to.GetSysDays())
        throw ApiError<400>{"invalid_period", "Начало периода не может быть позже окончания"};
    // Один запрос: сводка и все строки относятся к одному снимку данных.
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kSlave,
        sql::kCounterpartyReconciliation, organization, body.counterparty_id, body.own_company_id,
        body.contract_id, body.currency, body.date_from, body.date_to);
    Response200 response;
    for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag)) {
        if (row.IsTotal) {
            response.totals.push_back({.closing_minor = row.BalanceMinor,
                .credit_minor = row.CreditMinor,
                .currency = row.Currency,
                .debit_minor = row.DebitMinor,
                .opening_minor = row.OpeningMinor});
            continue;
        }
        if (response.operations.size() == 10000)
            throw ApiError<400>{"report_too_large",
                "В акте больше 10000 операций. Сузьте период или выберите своё юрлицо, договор и "
                "валюту"};
        response.operations.push_back({.balance_minor = row.BalanceMinor,
            .credit_minor = row.CreditMinor,
            .currency = row.Currency,
            .date = row.DocumentDate.value(),
            .debit_minor = row.DebitMinor,
            .id = boost::uuids::to_string(row.DocumentId.value()),
            .kind = models::ParseEnumScalar(row.Kind.value(), models::ReconciliationKind{}),
            .number = row.Number,
            .own_company_name = row.OwnCompanyName});
    }
    return response;
}
}  // namespace finance::views::api::v1::counterparty_reconciliation::get
