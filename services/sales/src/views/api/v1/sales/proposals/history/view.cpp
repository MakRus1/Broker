#include <components/proposals.hpp>
#include <views/api/v1/sales/proposals/history/view.hpp>

namespace sales::views::api::v1::sales::proposals::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.proposals->History(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::proposals::history
