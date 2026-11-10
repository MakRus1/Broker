#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/container_fact/view.hpp>

namespace transportations::views::api::v1::transportations::container_fact {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->ContainerFacts(std::move(request.body), context);
    return Response200{
        .groups = std::move(result.groups), .operations = std::move(result.operations)};
}
}  // namespace transportations::views::api::v1::transportations::container_fact
