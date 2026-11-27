#include <platform/browser/session.hpp>
#include <views/api/v1/voyages/get/view.hpp>

namespace web_bff::views::api::v1::voyages::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext& context,
                      Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto data = deps.shipments.ApiV1VoyagesGet(
        {.body = {.voyage_id = request.body.voyage_id}}, session.Access);
    Response200 result{};
    result.id = data.id;
    result.number = data.number;
    result.ports = data.ports;
    for (const auto& row : data.bookings)
        result.bookings.push_back({.booking_number = row.booking_number,
                                   .discharge_port = row.discharge_port,
                                   .load_port = row.load_port,
                                   .shipment_id = row.shipment_id,
                                   .shipment_number = row.shipment_number});
    return result;
}
}  // namespace web_bff::views::api::v1::voyages::get
