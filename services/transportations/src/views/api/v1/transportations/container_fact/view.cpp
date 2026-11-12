#include <components/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportations/container_fact/view.hpp>

namespace transportations::views::api::v1::transportations::container_fact {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    auto result = deps.transportations->GetContainerFacts(std::move(request.body), organization);

    return Response200{
        .groups = std::move(result.groups), .operations = std::move(result.operations)};
}
}  // namespace transportations::views::api::v1::transportations::container_fact
