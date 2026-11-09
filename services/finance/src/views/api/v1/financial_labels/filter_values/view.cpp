#include <components/financial_labels.hpp>
#include <utility>
#include <views/api/v1/financial_labels/filter_values/view.hpp>

namespace finance::views::api::v1::financial_labels::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.financial_labels->FilterValues(request.body, context);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace finance::views::api::v1::financial_labels::filter_values
