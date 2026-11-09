#include <components/proposals.hpp>
#include <views/api/v1/sales/proposals/list/view.hpp>

namespace sales::views::api::v1::sales::proposals::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.proposals->List(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::proposals::list
