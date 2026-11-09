#include <components/contacts.hpp>
#include <views/api/v1/sales/contacts/list/view.hpp>

namespace sales::views::api::v1::sales::contacts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.contacts->List(request.body, context);
    return Response200{.items = result.items, .next_before = result.next_before};
}
}  // namespace sales::views::api::v1::sales::contacts::list
