#include <platform/browser/session.hpp>
#include <views/api/preferences/roles/save/view.hpp>
namespace web_bff::views::api::preferences::roles::save {
Response View::Handle(Request&& request, userver::server::request::RequestContext& context,
                      Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiOrganizationsRegistryDefaultsSave;
    const auto result = deps.organizations.ApiOrganizationsRegistryDefaultsSave(
        api::Request{.body = {.expected_revision = request.body.expected_revision,
                              .organization_id = request.body.organization_id,
                              .registry = request.body.registry,
                              .role = request.body.role,
                              .value = request.body.value}},
        session.Access);
    return Response200{.item = {.registry = result.item.registry,
                                .revision = result.item.revision,
                                .role = result.item.role,
                                .value = result.item.value}};
}
}  // namespace web_bff::views::api::preferences::roles::save
