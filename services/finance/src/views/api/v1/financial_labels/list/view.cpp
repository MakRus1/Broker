#include <components/financial_labels.hpp>
#include <utility>
#include <views/api/v1/financial_labels/list/view.hpp>

namespace finance::views::api::v1::financial_labels::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.financial_labels->List(request.body, context);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace finance::views::api::v1::financial_labels::list
