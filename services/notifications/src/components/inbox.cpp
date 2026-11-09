#include <boost/uuid/string_generator.hpp>
#include <components/inbox.hpp>
#include <converters/notifications.hpp>
#include <notifications/sql_queries.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace notifications {
namespace pg = userver::storages::postgres;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

InboxComponent::InboxComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

userver::yaml_config::Schema InboxComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void InboxComponent::CheckOrganization(const userver::server::request::RequestContext& context,
    const std::string& organization) const {
    namespace api = clients::organizations::ApiOrganizationsAccess;
    try {
        organizations_.ApiOrganizationsAccess(
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

std::vector<models::Notification> InboxComponent::List(
    const models::ListNotificationsRequest& request,
    const userver::server::request::RequestContext& context) const {
    CheckOrganization(context, request.organization_id);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListNotifications,
        request.organization_id, identity.Issuer, identity.Subject, request.before, request.limit);
    std::vector<models::Notification> items;
    items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<converters::NotificationRow>(pg::kRowTag))
        items.push_back(converters::Convert(row));
    return items;
}

int InboxComponent::Read(const models::ReadNotificationsRequest& request,
    const userver::server::request::RequestContext& context) const {
    CheckOrganization(context, request.organization_id);
    const auto& identity = platform::auth::GetIdentity(context);
    std::vector<boost::uuids::uuid> ids;
    for (const auto& id : request.ids)
        ids.push_back(boost::uuids::string_generator{}(id));
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kReadNotifications,
        request.organization_id, identity.Issuer, identity.Subject, ids, request.dismiss);
    return static_cast<int>(rows.Size());
}
}  // namespace notifications
