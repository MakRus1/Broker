#include <converters/items.hpp>
#include <converters/transportations.hpp>
#include <platform/browser/session.hpp>
#include <platform/http_client.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <views/api/v1/transportations/list/view.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace web_bff::views::api::v1::transportations::list {
using namespace ::web_bff::converters;

namespace {
using ShipmentReferences = std::vector<models::transportations::TransportationShipmentReference>;

template <typename Filter> std::vector<Filter> IdFilter(const std::set<boost::uuids::uuid>& ids) {
    std::vector<std::string> values;
    values.reserve(ids.size());
    for (const auto& id : ids)
        values.push_back(userver::utils::ToString(id));
    return {{.field = "id", .values = std::move(values)}};
}

// Номер поставки и имя клиента нужны только для отображения строки реестра.
// Их отсутствие не должно ломать сам список, поэтому ошибки чтения оставляют строки без них.
// Права проверяют сервисы поставок и контрагентов: без права чтения они отвечают 403.
template <typename Call> bool TryRead(std::string_view what, Call&& call) {
    try {
        call();
        return true;
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ErrorByStatus<403>&) {
        LOG_DEBUG() << "Transportation registry: no access to " << what;
    } catch (const platform::http::ApiError& error) {
        LOG_WARNING() << "Transportation registry: " << what << " unavailable, HTTP "
                      << error.GetStatus();
    } catch (const platform::http::TransportError& error) {
        LOG_WARNING() << "Transportation registry: " << what << " unavailable: " << error.what();
    } catch (const platform::http::ContractError& error) {
        LOG_WARNING() << "Transportation registry: " << what << " invalid: " << error.what();
    }
    return false;
}

std::optional<ShipmentReferences> ReadShipmentReferences(
    const std::vector<clients::transportations::models::Transportation>& items,
    const platform::browser::BrowserSession& session, Deps& deps) {
    if (items.empty())
        return std::nullopt;

    std::set<boost::uuids::uuid> shipment_ids;
    for (const auto& item : items)
        shipment_ids.insert(item.shipment_id);

    std::vector<clients::shipments::models::Shipment> shipments;
    const auto shipments_read = TryRead("shipments", [&] {
        namespace api = clients::shipments::ApiV1ShipmentsList;
        auto result = deps.shipments.ApiV1ShipmentsList(
            api::Request{.body = {.filters = IdFilter<clients::shipments::models::RegistryFilter>(
                                      shipment_ids),
                             .limit = static_cast<std::int64_t>(shipment_ids.size())}},
            session.Access);
        shipments = std::move(result.items);
    });
    if (!shipments_read || shipments.empty())
        return std::nullopt;

    std::map<boost::uuids::uuid, std::string> customer_names;
    {
        std::set<boost::uuids::uuid> customer_ids;
        for (const auto& shipment : shipments)
            customer_ids.insert(shipment.customer_id);
        TryRead("customers", [&] {
            namespace api = clients::counterparties::ApiV1CounterpartiesList;
            const auto result = deps.counterparties.ApiV1CounterpartiesList(
                api::Request{
                    .body = {.filters = IdFilter<clients::counterparties::models::RegistryFilter>(
                                 customer_ids),
                        .limit = static_cast<std::int64_t>(customer_ids.size())}},
                session.Access);
            for (const auto& customer : result.items)
                customer_names.emplace(customer.id, customer.name);
        });
    }

    ShipmentReferences references;
    references.reserve(shipments.size());
    for (const auto& shipment : shipments) {
        models::transportations::TransportationShipmentReference reference;
        reference.id = shipment.id;
        reference.number = shipment.number;
        if (const auto name = customer_names.find(shipment.customer_id);
            name != customer_names.end())
            reference.customer_name = name->second;
        references.push_back(std::move(reference));
    }
    return references;
}
}  // namespace

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
    return Response200{
        .items = ConvertItems(result.items, [](const auto& item) { return Convert(item); }),
        .next_cursor = cursor,
        .shipments = ReadShipmentReferences(result.items, session, deps)};
}
}  // namespace web_bff::views::api::v1::transportations::list
