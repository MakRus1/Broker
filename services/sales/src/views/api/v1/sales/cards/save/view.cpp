#include <components/cards.hpp>
#include <views/api/v1/sales/cards/save/view.hpp>

namespace sales::views::api::v1::sales::cards::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.cards->Save(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::cards::save
