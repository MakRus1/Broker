#include <clients/counterparties.hpp>
#include <clients/shipments.hpp>
#include <converters/items.hpp>
#include <converters/transportation_registry.hpp>
#include <converters/transportations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace web_bff::views::api::v1::transportations::list {
using namespace ::web_bff::converters;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationsList;
    const auto result = deps.transportations.ApiV1TransportationsList(
        api::Request{.body = Convert(request.body)}, session.Access);
    std::optional<models::transportations::RegistryCursor> cursor;
    if (result.next_cursor.has_value())
        cursor = models::transportations::RegistryCursor{.id = result.next_cursor.value().id,
            .scope = result.next_cursor.value().scope,
            .value = result.next_cursor.value().value};

    // Номер поставки и клиент нужны строкам реестра. Без права чтения их просто нет.
    const auto shipments =
        clients::shipments::ListByIds(deps.shipments, ShipmentIds(result.items), session.Access);
    const auto customers = clients::counterparties::ListByIds(
        deps.counterparties, CustomerIds(shipments), session.Access);

    return Response200{
        .items = ConvertItems(result.items, [](const auto& item) { return Convert(item); }),
        .next_cursor = cursor,
        .shipments = ToShipmentReferences(shipments, customers)};
}
}  // namespace web_bff::views::api::v1::transportations::list
