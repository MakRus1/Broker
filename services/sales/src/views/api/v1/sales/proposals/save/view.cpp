#include <components/proposals.hpp>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace sales::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.proposals->Save(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::proposals::save
