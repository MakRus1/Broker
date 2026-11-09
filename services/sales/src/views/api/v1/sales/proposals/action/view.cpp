#include <components/proposals.hpp>
#include <views/api/v1/sales/proposals/action/view.hpp>

namespace sales::views::api::v1::sales::proposals::action {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.proposals->Action(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::proposals::action
