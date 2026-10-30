#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace shipments::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;

    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOperation, organization, request.body.operation_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Операция не найдена"};
    const auto result = Convert(rows.AsSingleRow<pgtypes::CreationOperationV1>());
    return Response200{.operation = result};
}
}  // namespace shipments::views::api::v1::transportation_operations::get
