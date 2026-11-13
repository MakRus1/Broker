#pragma once
#include <converters/finance_settings.hpp>
#include <finance/sql_queries.hpp>
#include <openapi/models.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline models::FinanceSettings ReadFinanceSettings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.incoming_number_template = "ВС-{sequence}/{yyyy}",
            .outgoing_number_template = "ИС-{sequence}/{yyyy}",
            .reporting_currency = "RUB",
            .reset_numbers_yearly = false,
            .revision = 0};
    return converters::ToFinanceSettings(rows.AsSingleRow<models::FinanceSettingsRow>(pg::kRowTag));
}

inline models::FinanceSettings LoadFinanceSettings(
    const pg::ClusterPtr& cluster, const std::string& organization) {
    return ReadFinanceSettings(
        cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetFinanceSettings, organization));
}

}  // namespace finance::helpers
