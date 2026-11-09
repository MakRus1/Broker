#include <components/service_charges.hpp>
#include <utility>
#include <views/api/internal/v1/service_charges/apply_import/view.hpp>

namespace finance::views::api::internal::v1::service_charges::apply_import {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.imported = deps.service_charges->ApplyImport(request.body, context)};
}
}  // namespace finance::views::api::internal::v1::service_charges::apply_import
