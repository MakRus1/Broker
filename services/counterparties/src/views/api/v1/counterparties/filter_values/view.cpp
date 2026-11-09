#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/filter_values/view.hpp>

namespace counterparties::views::api::v1::counterparties::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->FilterValues(request.body, context);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace counterparties::views::api::v1::counterparties::filter_values
