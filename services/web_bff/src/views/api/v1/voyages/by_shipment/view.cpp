#include <platform/browser/session.hpp>
#include <views/api/v1/voyages/by_shipment/view.hpp>

namespace web_bff::views::api::v1::voyages::by_shipment {
Response View::Handle(Request&& request, userver::server::request::RequestContext& context,
                      Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto data = deps.shipments.ApiV1VoyagesByShipment(
        {.body = {.shipment_id = request.body.shipment_id}}, session.Access);
    Response200 result{};
    for (const auto& row : data.items)
        result.items.push_back({.discharge_port = row.discharge_port,
                                .load_port = row.load_port,
                                .segment_id = row.segment_id,
                                .voyage_id = row.voyage_id,
                                .voyage_number = row.voyage_number});
    return result;
}
}  // namespace web_bff::views::api::v1::voyages::by_shipment
