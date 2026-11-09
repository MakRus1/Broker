#include <components/service_charges.hpp>
#include <utility>
#include <views/api/v1/service_charges/get/view.hpp>

namespace finance::views::api::v1::service_charges::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.charge = deps.service_charges->Get(request.body, context)};
}
}  // namespace finance::views::api::v1::service_charges::get
