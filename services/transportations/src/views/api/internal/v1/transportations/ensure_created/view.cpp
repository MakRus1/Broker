#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::transportations::models::EnsureCreatedRequest command{
        .container_ownership = request.body.container_ownership,
        .container_type_code = request.body.container_type_code,
        .item_id = request.body.item_id,
        .operation_id = request.body.operation_id,
        .quantity = request.body.quantity,
        .schema_version = request.body.schema_version,
        .shipment_id = request.body.shipment_id,
        .tenant_id = request.body.tenant_id};
    try {
        return Response200{.operation = ::transportations::Store{deps.pg_cluster}.EnsureCreated(
                               platform::auth::GetPrincipal(context), command)};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::internal::v1::transportations::ensure_created
