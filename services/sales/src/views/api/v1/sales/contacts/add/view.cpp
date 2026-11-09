#include <components/contacts.hpp>
#include <views/api/v1/sales/contacts/add/view.hpp>

namespace sales::views::api::v1::sales::contacts::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.contacts->Add(request.body, context);
    return Response200{.card = result.card, .item = result.item};
}
}  // namespace sales::views::api::v1::sales::contacts::add
