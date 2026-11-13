#include <components/service_charges.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/internal/v1/service_charges/apply_import/view.hpp>

namespace finance::views::api::internal::v1::service_charges::apply_import {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);

    return Response200{
        .imported = deps.service_charges->ApplyServiceChargeImport(identity, request.body)};
}
}  // namespace finance::views::api::internal::v1::service_charges::apply_import
