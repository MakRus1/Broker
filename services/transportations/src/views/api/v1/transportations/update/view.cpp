#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/details.hpp>
#include <helpers/inherited_route.hpp>
#include <helpers/revision.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/update/view.hpp>

namespace transportations::views::api::v1::transportations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    ValidateDetails(request.body.type, request.body.container_type_code,
        request.body.container_ownership, request.body.container_number);
    auto loaded = LoadTransportation(deps, context, request.body.transportation_id);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto current = ReadTransportation(transaction, organization, loaded.id, true);
    CheckRevision(current.revision, request.body.expected_revision);
    auto result = Convert(
        transaction
            .Execute(sql::kUpdateTransportation, organization, loaded.id,
                models::EnumValue(request.body.type), request.body.hazardous,
                request.body.container_type_code, Ownership(request.body.container_ownership),
                CleanContainerNumber(request.body.container_number))
            .AsSingleRow<pgtypes::TransportationV4>());
    result.route_segments = std::move(loaded.route_segments);
    result.incoterms = loaded.incoterms;
    result.loading = loaded.loading;
    result.carriage_terms = loaded.carriage_terms;
    WriteAudit(transaction, organization, identity, "update_transportation", result.id,
        request.body, result);
    transaction.Commit();
    return Response200{.transportation = result};
}
}  // namespace transportations::views::api::v1::transportations::update
