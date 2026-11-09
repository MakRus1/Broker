#pragma once
#include <components/users.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class MembersComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-members";
    MembersComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::MembersListResult List(
        const models::ListMembersRequest&, userver::server::request::RequestContext&) const;
    models::MemberResult Create(
        const models::CreateMemberRequest&, userver::server::request::RequestContext&) const;
    models::MemberResult Add(
        const models::AddMemberRequest&, userver::server::request::RequestContext&) const;
    models::MemberResult Update(
        const models::UpdateMemberRequest&, userver::server::request::RequestContext&) const;
    models::MemberResult UpdateProfile(
        const models::UpdateMemberProfileRequest&, userver::server::request::RequestContext&) const;

  private:
    const Users& users_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::MembersComponent> = true;
