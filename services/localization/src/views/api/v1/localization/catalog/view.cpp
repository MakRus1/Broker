#include <components/catalog.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/localization/catalog/view.hpp>

namespace localization::views::api::v1::localization::catalog {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.messages = deps.catalog->GetOrganizationCatalog(
                           request.body, platform::auth::GetAccessToken(context))};
}
}  // namespace localization::views::api::v1::localization::catalog
