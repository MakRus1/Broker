#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/filter_values/view.hpp>

namespace transportations::views::api::v1::transportations::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->FilterValues(std::move(request.body), context);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace transportations::views::api::v1::transportations::filter_values
