#include <components/preferences.hpp>
#include <converters/preferences.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <platform/http_client.hpp>
#include <user_preferences/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace user_preferences {
namespace pg = userver::storages::postgres;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

PreferencesComponent::PreferencesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

userver::yaml_config::Schema PreferencesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void PreferencesComponent::CheckOrganization(
    const std::string& access_token, const std::string& organization) const {
    namespace api = clients::organizations::ApiOrganizationsMembershipGet;
    try {
        organizations_.ApiOrganizationsMembershipGet(
            api::Request{.body = {.organization_id = organization}}, access_token);
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

std::vector<models::Preference> PreferencesComponent::ListPreferences(
    const models::ListPreferencesRequest& request, const platform::auth::Identity& identity,
    const std::string& access_token) const {
    CheckOrganization(access_token, request.organization_id);

    return converters::Convert(cluster_->Execute(pg::ClusterHostType::kMaster,
        sql::kListPreferences, request.organization_id, identity.Issuer, identity.Subject));
}

models::Preference PreferencesComponent::SavePreference(
    const models::SavePreferenceRequest& request, const platform::auth::Identity& identity,
    const std::string& access_token) const {
    CheckOrganization(access_token, request.organization_id);

    if (userver::formats::json::ToString(request.value).size() > 65536)
        throw ApiError<400>{"preference_too_large", "Настройка не должна превышать 64 КиБ"};

    const auto rows = request.expected_revision == 0
        ? cluster_->Execute(pg::ClusterHostType::kMaster, sql::kCreatePreference,
              request.organization_id, identity.Issuer, identity.Subject, request.key,
              request.value)
        : cluster_->Execute(pg::ClusterHostType::kMaster, sql::kUpdatePreference,
              request.organization_id, identity.Issuer, identity.Subject, request.key,
              request.value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"preference_changed", "Настройка изменена на другом устройстве"};
    return converters::Convert(rows.AsSingleRow<models::PreferenceRow>(pg::kRowTag));
}
}  // namespace user_preferences
