#pragma once
#include <components/keycloak_users.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class MembersComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-members";
    MembersComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::MembersListResult ListMembers(
        const models::ListMembersRequest&, const platform::auth::Identity&) const;
    models::MemberResult CreateMember(
        const models::CreateMemberRequest&, const platform::auth::Identity&) const;
    models::MemberResult AddExistingMember(
        const models::AddMemberRequest&, const platform::auth::Identity&) const;
    models::MemberResult UpdateMemberAccess(
        const models::UpdateMemberRequest&, const platform::auth::Identity&) const;
    models::MemberResult UpdateMemberProfile(
        const models::UpdateMemberProfileRequest&, const platform::auth::Identity&) const;

  private:
    const KeycloakUsers& keycloak_users_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::MembersComponent> = true;
