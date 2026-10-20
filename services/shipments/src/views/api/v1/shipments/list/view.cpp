#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/shipments/list/view.hpp>

namespace shipments::views::api::v1::shipments::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["limit"] = request.body.limit;
    if (request.body.after)
        body["after"] = *request.body.after;
    try {
        const auto result = ::shipments::Store{deps.pg_cluster}.ListShipments(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        std::vector<::shipments::models::Shipment> items;
        for (const auto& item : result["items"])
            items.push_back(::shipments::models::ParseModelJson(
                item, std::type_identity<::shipments::models::Shipment>{}));
        return Response200{.items = std::move(items)};
    } catch (const ::shipments::ApiError& error) {
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
}  // namespace shipments::views::api::v1::shipments::list
