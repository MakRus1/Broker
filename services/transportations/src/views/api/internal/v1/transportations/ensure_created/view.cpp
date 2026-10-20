#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["operation_id"] = request.body.operation_id;
    body["shipment_id"] = request.body.shipment_id;
    body["item_id"] = request.body.item_id;
    body["tenant_id"] = request.body.tenant_id;
    body["quantity"] = request.body.quantity;
    body["schema_version"] = request.body.schema_version;
    body["container_type_code"] = request.body.container_type_code;
    body["container_ownership"] = request.body.container_ownership;
    try {
        const auto result = ::transportations::Store{deps.pg_cluster}.EnsureCreated(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        return Response200{.operation = ::transportations::models::ParseModelJson(result,
                               std::type_identity<::transportations::models::CreationOperation>{})};
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
