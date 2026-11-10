#include <components/transportations.hpp>
#include <utility>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->EnsureCreated(std::move(request.body), context);
    return Response200{.operation = std::move(result.operation)};
}
}  // namespace transportations::views::api::internal::v1::transportations::ensure_created
