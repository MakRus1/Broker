#pragma once
#include <deps.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/exchange_amounts.hpp>
#include <openapi/models.hpp>
#include <platform/auth/organization_admin.hpp>
#include <tuple>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline models::FinanceSettings GetFinanceSettings(
    const Deps& deps, const std::string& organization) {
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetFinanceSettings, organization);
    if (rows.IsEmpty())
        return {.reporting_currency = "RUB", .revision = 0};
    const auto [currency, revision] =
        rows.AsSingleRow<std::tuple<std::string, std::int64_t>>(pg::kRowTag);
    return {.reporting_currency = currency, .revision = revision};
}

inline void RequireFinanceAdministrator(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    try {
        platform::auth::RequireOrganizationAdministrator(deps.organizations, context, organization);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }
}
}  // namespace finance::helpers
