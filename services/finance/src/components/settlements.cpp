#include <components/settlements.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/financial_labels.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/date_validation.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

SettlementsComponent::SettlementsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                              .GetCluster()} {}

userver::yaml_config::Schema SettlementsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

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

models::ReconciliationResponse SettlementsComponent::Reconciliation(
    const models::ReconciliationRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& body = request;
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
    models::ReconciliationResponse response;
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

models::CounterpartySettlementsResponse SettlementsComponent::Balances(
    const models::CounterpartySettlementsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    // Один SQL-запрос даёт согласованный снимок счетов и распределений платежей.
    const auto rows = deps.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
        sql::kCounterpartySettlements, organization, body.counterparty_id, body.own_company_id,
        body.contract_id, body.currency);
    models::CounterpartySettlementsResponse response;
    for (const auto& row : rows) {
        const auto [currency, receivable, payable, overdue_receivable, overdue_payable,
            unallocated_receipts, unallocated_disbursements] = row.As<std::string, std::string,
            std::string, std::string, std::string, std::string, std::string>();
        response.totals.push_back({.currency = currency,
            .overdue_payable_minor = overdue_payable,
            .overdue_receivable_minor = overdue_receivable,
            .payable_minor = payable,
            .receivable_minor = receivable,
            .unallocated_disbursements_minor = unallocated_disbursements,
            .unallocated_receipts_minor = unallocated_receipts});
    }
    return response;
}
}  // namespace finance
