#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace notifications {
class InboxComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "notifications-inbox";
    InboxComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::Notification> List(const models::ListNotificationsRequest&,
        const userver::server::request::RequestContext&) const;
    int Read(const models::ReadNotificationsRequest&,
        const userver::server::request::RequestContext&) const;

  private:
    void CheckOrganization(
        const userver::server::request::RequestContext&, const std::string&) const;
    userver::storages::postgres::ClusterPtr cluster_;
    clients::organizations::Client organizations_;
};
}  // namespace notifications

template <>
inline constexpr bool userver::components::kHasValidate<notifications::InboxComponent> = true;
