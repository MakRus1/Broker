#include <boost/uuid/uuid_io.hpp>
#include <helpers/database.hpp>
#include <postgresql/types.hpp>
#include <views/api/v1/transportations/container_fact/view.hpp>

namespace transportations::views::api::v1::transportations::container_fact {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kContainerFacts,
        organization, request.body.shipment_id, request.body.pending_operation_ids);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::ContainerFactV1>()) {
        if (row.OperationId.has_value()) {
            result.operations.push_back({.created_quantity = row.Quantity,
                .operation_id = boost::uuids::to_string(row.OperationId.value())});
            continue;
        }
        models::ContainerFact fact;
        fact.container_type_code = row.ContainerType;
        if (row.Ownership.has_value())
            fact.container_ownership =
                models::ParseEnumScalar(row.Ownership.value(), models::ContainerOwnership{});
        fact.quantity = row.Quantity;
        result.groups.push_back(std::move(fact));
    }
    return result;
}
}  // namespace transportations::views::api::v1::transportations::container_fact
