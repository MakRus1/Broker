#include <components/rfq.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/rfqs/save/view.hpp>

namespace sales::views::api::v1::sales::rfqs::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.rfq->Save(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::rfqs::save
