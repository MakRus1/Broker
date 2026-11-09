#include <components/cards.hpp>
#include <views/api/v1/sales/cards/get/view.hpp>

namespace sales::views::api::v1::sales::cards::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.cards->Get(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::cards::get
