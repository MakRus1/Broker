#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/get/view.hpp>

namespace counterparties::views::api::v1::counterparties::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->Get(request.body, context);
    return Response200{.counterparty = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::get
