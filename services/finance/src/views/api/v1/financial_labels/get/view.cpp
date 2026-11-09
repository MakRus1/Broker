#include <components/financial_labels.hpp>
#include <utility>
#include <views/api/v1/financial_labels/get/view.hpp>

namespace finance::views::api::v1::financial_labels::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.label = deps.financial_labels->Get(request.body, context)};
}
}  // namespace finance::views::api::v1::financial_labels::get
