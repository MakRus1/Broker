#include <clients/counterparties.hpp>
#include <clients/shipments.hpp>
#include <converters/items.hpp>
#include <converters/transportations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/list/view.hpp>

#include <map>
#include <set>
#include <string>

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
    std::set<boost::uuids::uuid> shipment_ids;
    for (const auto& item : result.items)
        shipment_ids.insert(item.shipment_id);
    const auto shipments =
        clients::shipments::ListByIds(deps.shipments, shipment_ids, session.Access);

    std::set<boost::uuids::uuid> customer_ids;
    for (const auto& shipment : shipments)
        customer_ids.insert(shipment.customer_id);
    std::map<boost::uuids::uuid, std::string> customer_names;
    for (const auto& customer :
        clients::counterparties::ListByIds(deps.counterparties, customer_ids, session.Access))
        customer_names.emplace(customer.id, customer.name);

    std::optional<std::vector<models::transportations::TransportationShipmentReference>> references;
    if (!shipments.empty())
        references = ConvertItems(shipments, [&](const auto& shipment) {
            models::transportations::TransportationShipmentReference reference;
            reference.id = shipment.id;
            reference.number = shipment.number;
            if (const auto name = customer_names.find(shipment.customer_id);
                name != customer_names.end())
                reference.customer_name = name->second;
            return reference;
        });

    return Response200{
        .items = ConvertItems(result.items, [](const auto& item) { return Convert(item); }),
        .next_cursor = cursor,
        .shipments = std::move(references)};
}
}  // namespace web_bff::views::api::v1::transportations::list
