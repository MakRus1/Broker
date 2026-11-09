#include <components/contacts.hpp>
#include <views/api/v1/sales/contacts/update/view.hpp>

namespace sales::views::api::v1::sales::contacts::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.contacts->Update(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::contacts::update
