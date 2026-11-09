#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->Create(request.body, context);
    return Response201{.counterparty = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::create
