#include <components/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportations/history/view.hpp>

namespace transportations::views::api::v1::transportations::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    auto result =
        deps.transportations->GetTransportationHistory(std::move(request.body), organization);

    return Response200{.items = std::move(result.items)};
}
}  // namespace transportations::views::api::v1::transportations::history
