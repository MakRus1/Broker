#pragma once
#include <deps.hpp>
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <platform/http_client.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace notifications::helpers {
namespace pg = userver::storages::postgres;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

inline void CheckOrganization(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    namespace api = clients::organizations::ApiOrganizationsAccess;
    try {
        deps.organizations.ApiOrganizationsAccess(
            api::Request{.body = {.organization_id = organization, .permission = "shipments.read"}},
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
}  // namespace notifications::helpers
