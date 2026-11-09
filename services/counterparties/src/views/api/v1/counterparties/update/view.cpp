#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/update/view.hpp>

namespace counterparties::views::api::v1::counterparties::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->Update(request.body, context);
    return Response200{.counterparty = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::update
