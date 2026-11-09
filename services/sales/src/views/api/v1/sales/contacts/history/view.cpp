#include <components/contacts.hpp>
#include <views/api/v1/sales/contacts/history/view.hpp>

namespace sales::views::api::v1::sales::contacts::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.contacts->History(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::contacts::history
