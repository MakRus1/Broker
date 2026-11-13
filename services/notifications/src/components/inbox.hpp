#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace notifications {
class InboxComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "notifications-inbox";
    InboxComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::Notification> ListNotifications(const models::ListNotificationsRequest&,
        const platform::auth::Identity&, const std::string& access_token) const;
    int MarkNotificationsRead(const models::ReadNotificationsRequest&,
        const platform::auth::Identity&, const std::string& access_token) const;

  private:
    void CheckOrganization(const std::string& access_token, const std::string&) const;
    userver::storages::postgres::ClusterPtr cluster_;
    clients::organizations::Client organizations_;
};
}  // namespace notifications

template <>
inline constexpr bool userver::components::kHasValidate<notifications::InboxComponent> = true;
