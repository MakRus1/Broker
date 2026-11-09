#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/save/view.hpp>

namespace sales::views::api::v1::sales::inquiries::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.inquiries->Save(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::inquiries::save
