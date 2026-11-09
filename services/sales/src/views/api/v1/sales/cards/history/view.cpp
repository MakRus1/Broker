#include <components/cards.hpp>
#include <views/api/v1/sales/cards/history/view.hpp>

namespace sales::views::api::v1::sales::cards::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.cards->History(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::cards::history
