#include <helpers/service_charges.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/service_charges/get/view.hpp>

namespace finance::views::api::v1::service_charges::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetServiceCharge, organization, request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
    ValidateServiceChargeScope(deps, context,
        {.shipment_id = charge.shipment_id, .transportation_id = charge.transportation_id});
    return Response200{.charge = charge};
}
}  // namespace finance::views::api::v1::service_charges::get
