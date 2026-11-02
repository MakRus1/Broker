#pragma once
#include <deps.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <platform/http_client.hpp>
#include <user_preferences/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace user_preferences::helpers {
namespace pg = userver::storages::postgres;

struct PreferenceRow {
    std::string Key;
    userver::formats::json::Value Value;
    std::int64_t Revision;
};

inline models::Preference Convert(const PreferenceRow& row) {
    return {.key = row.Key, .revision = row.Revision, .value = row.Value};
}

inline std::vector<models::Preference> Convert(const pg::ResultSet& rows) {
    std::vector<models::Preference> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<PreferenceRow>(pg::kRowTag))
        result.push_back(Convert(row));
    return result;
}

template <int Status> using ApiError = platform::http::HandlerError<Status>;

inline void CheckOrganization(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    namespace api = clients::organizations::ApiOrganizationsMembershipGet;
    try {
        deps.organizations.ApiOrganizationsMembershipGet(
            api::Request{.body = {.organization_id = organization}},
            platform::auth::GetAccessToken(context));
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }
}
}  // namespace user_preferences::helpers
