#include <components/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);

    auto result = deps.transportations->CreateTransportationsFromCommand(
        request.body, platform::auth::GetIdentity(context));

    return Response200{.operation = std::move(result.operation)};
}
}  // namespace transportations::views::api::internal::v1::transportations::ensure_created
