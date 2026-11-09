#pragma once
#include <finance/sql_queries.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <tuple>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline models::FinanceSettings ConvertFinanceSettings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.incoming_number_template = "ВС-{sequence}/{yyyy}",
            .outgoing_number_template = "ИС-{sequence}/{yyyy}",
            .reporting_currency = "RUB",
            .reset_numbers_yearly = false,
            .revision = 0};
    const auto [currency, revision, outgoing, incoming, yearly] =
        rows.AsSingleRow<std::tuple<std::string, std::int64_t, std::string, std::string, bool>>(
            pg::kRowTag);
    return {.incoming_number_template = incoming,
        .outgoing_number_template = outgoing,
        .reporting_currency = currency,
        .reset_numbers_yearly = yearly,
        .revision = revision};
}

inline models::FinanceSettings GetFinanceSettings(
    const pg::ClusterPtr& cluster, const std::string& organization) {
    return ConvertFinanceSettings(
        cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetFinanceSettings, organization));
}

}  // namespace finance::helpers
