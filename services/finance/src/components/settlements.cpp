#include <components/settlements.hpp>
#include <converters/financial_label.hpp>
#include <finance/sql_queries.hpp>
#include <models/errors.hpp>
#include <models/reconciliation_row.hpp>
#include <models/settlement_total_row.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/date_validation.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
namespace pg = userver::storages::postgres;

SettlementsComponent::SettlementsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema SettlementsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ReconciliationResponse SettlementsComponent::GetReconciliation(
    const std::string& organization, const models::ReconciliationRequest& request) const {

    const auto& body = request;
    for (const auto date : {body.date_from, body.date_to})
        if (!platform::IsCalendarDateInRange(
                date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (body.date_from.GetSysDays() > body.date_to.GetSysDays())
        throw ApiError<400>{"invalid_period", "Начало периода не может быть позже окончания"};
    // Один запрос: сводка и все строки относятся к одному снимку данных.
    const auto rows = cluster_->Execute(pg::ClusterHostType::kSlave,
        sql::kCounterpartyReconciliation, organization, body.counterparty_id, body.own_company_id,
        body.contract_id, body.currency, body.date_from, body.date_to);
    models::ReconciliationResponse response;
    for (const auto& row : rows.AsSetOf<models::ReconciliationRow>(pg::kRowTag)) {
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

models::CounterpartySettlementsResponse SettlementsComponent::GetBalances(
    const std::string& organization, const models::CounterpartySettlementsRequest& request) const {

    const auto& body = request;
    // Один SQL-запрос даёт согласованный снимок счетов и распределений платежей.
    const auto rows = cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave,
        sql::kCounterpartySettlements, organization, body.counterparty_id, body.own_company_id,
        body.contract_id, body.currency);
    models::CounterpartySettlementsResponse response;
    for (const auto& row : rows) {
        const auto total = row.As<models::SettlementTotalRow>(pg::kRowTag);
        response.totals.push_back({.currency = total.Currency,
            .overdue_payable_minor = total.OverduePayable,
            .overdue_receivable_minor = total.OverdueReceivable,
            .payable_minor = total.Payable,
            .receivable_minor = total.Receivable,
            .unallocated_disbursements_minor = total.UnallocatedDisbursements,
            .unallocated_receipts_minor = total.UnallocatedReceipts});
    }
    return response;
}
}  // namespace finance
