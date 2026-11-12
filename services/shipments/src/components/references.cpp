#include <algorithm>
#include <components/references.hpp>
#include <converters/route_details.hpp>
#include <converters/route_locations.hpp>
#include <domain/route_details.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;
using namespace converters;

void ReferencesComponent::ResolveRoute(const userver::server::request::RequestContext& context,
    std::vector<models::ShipmentRouteSegment>& segments,
    const std::vector<models::ShipmentRouteSegment>& previous) const {
    const auto locations = LoadRouteLocations(context, segments);
    domain::ResolveRouteDetails(segments, previous, locations);
    ValidateRouteCounterparties(context, segments, previous);
}

ReferencesComponent::ReferencesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      transport_locations_(
          context.FindComponent<clients::transport_locations::Component>().GetClient()),
      transportations_(context.FindComponent<clients::transportations::Component>().GetClient()) {}

userver::yaml_config::Schema ReferencesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

std::vector<clients::organizations::models::OrganizationEmployee>
ReferencesComponent::FindEmployees(const userver::server::request::RequestContext& context,
    const std::optional<std::string>& query, const std::optional<std::string>& subject,
    int limit) const try {
    namespace api = clients::organizations::ApiOrganizationsEmployeesList;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto response = organizations_.ApiOrganizationsEmployeesList(
        api::Request{.body = {.limit = limit,
                         .organization_id = platform::auth::GetOrganization(identity),
                         .query = query,
                         .subject = subject}},
        platform::auth::GetAccessToken(context));
    return response.items;
} catch (const platform::http::ApiError&) {

    throw ApiError<503>{"employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{"employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "employee_directory_unavailable", "Некорректный ответ справочника сотрудников"};
}

void ReferencesComponent::ValidateLogist(const userver::server::request::RequestContext& context,
    const std::optional<std::string>& subject) const {
    if (subject.has_value() && FindEmployees(context, std::nullopt, subject, 1).empty())
        throw ApiError<400>{"invalid_logist", "Выберите действующего сотрудника организации"};
}

std::optional<std::string> ReferencesComponent::CustomerLogist(
    const userver::server::request::RequestContext& context, const std::string& customer) const
    try {
    namespace api = clients::counterparties::ApiV1CounterpartiesGet;
    const auto result = counterparties_.ApiV1CounterpartiesGet(
        api::Request{.body = {.id = customer}}, platform::auth::GetAccessToken(context));
    return result.counterparty.logist.value_or(std::nullopt);
} catch (const platform::http::ErrorByStatus<404>&) {

    throw ApiError<400>{"invalid_customer", "Клиент не найден в организации"};
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{"customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{"customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "customer_directory_unavailable", "Некорректный ответ справочника контрагентов"};
}

inline std::set<std::string> RouteCounterparties(
    const std::vector<models::ShipmentRouteSegment>& stages) {
    std::set<std::string> ids;
    const auto add = [&](const auto& id) {
        if (id.has_value())
            ids.insert(id.value());
    };
    for (const auto& stage : stages) {
        if (stage.sea.has_value()) {
            add(stage.sea.value().departure_terminal_id);
            add(stage.sea.value().arrival_terminal_id);
        }
        if (stage.air.has_value())
            add(stage.air.value().carrier_id);
    }
    return ids;
}

void ReferencesComponent::ValidateRouteCounterparties(
    const userver::server::request::RequestContext& context,
    const std::vector<models::ShipmentRouteSegment>& stages,
    const std::vector<models::ShipmentRouteSegment>& previous) const {
    const auto old = RouteCounterparties(previous);
    for (const auto& id : RouteCounterparties(stages)) {
        if (old.contains(id))
            continue;
        try {
            const auto response = counterparties_.ApiV1CounterpartiesGet(
                {.body = {.id = id}}, platform::auth::GetAccessToken(context));
            if (response.counterparty.archived)
                throw ApiError<400>{
                    "invalid_route_counterparty", "Выберите действующего контрагента организации"};
        } catch (const platform::http::ErrorByStatus<404>&) {
            throw ApiError<400>{"invalid_route_counterparty", "Контрагент не найден в организации"};
        } catch (const platform::http::ApiError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::CancelledError&) {
            throw;
        } catch (const platform::http::TransportError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::ContractError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Некорректный ответ справочника контрагентов"};
        }
    }
}

using converters::Convert;

domain::RouteLocations ReferencesComponent::LoadRouteLocations(
    const userver::server::request::RequestContext& context,
    const std::vector<models::ShipmentRouteSegment>& segments) const try {
    std::set<boost::uuids::uuid> ids;
    const auto add = [&](const auto& location) {
        if (location.has_value())
            ids.insert(boost::uuids::string_generator{}(location.value().id));
    };
    for (const auto& segment : segments) {
        if (segment.sea.has_value()) {
            add(segment.sea.value().pol);
            add(segment.sea.value().pod);
        }
        if (segment.air.has_value()) {
            add(segment.air.value().origin_airport);
            add(segment.air.value().destination_airport);
        }
    }
    domain::RouteLocations result;
    if (ids.empty())
        return result;
    std::vector<std::string> requested;
    for (const auto& id : ids)
        requested.push_back(boost::uuids::to_string(id));
    const auto response = transport_locations_.ApiV1RouteLocationsGetBatch(
        {.body = {.ids = requested}}, platform::auth::GetAccessToken(context));
    for (const auto& value : response.items) {
        auto location = converters::Convert(value);
        result.emplace(location.id, std::move(location));
    }
    return result;
} catch (const platform::http::CancelledError&) {

    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Не удалось проверить транспортные пункты"};
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Справочник транспортных пунктов недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Некорректный ответ справочника транспортных пунктов"};
}

void ReferencesComponent::CheckExistingRouteUsage(
    const userver::server::request::RequestContext& context, const std::string& shipment_id,
    const std::vector<std::string>& changed) const try {
    if (changed.empty())
        return;
    namespace api = clients::transportations::ApiV1TransportationRoutesUsage;
    const auto result = transportations_.ApiV1TransportationRoutesUsage(
        api::Request{.body = {.shipment_id = shipment_id}},
        platform::auth::GetAccessToken(context));
    for (const auto& id : result.segment_ids)
        if (std::find(changed.begin(), changed.end(), id) != changed.end())
            throw ApiError<409>{"route_segment_in_use",
                "Нельзя удалить или изменить тип участка с данными перевозок"};
} catch (const platform::http::HandlerErrorBase&) {

    throw;
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const std::exception&) {
    throw ApiError<503>{
        "transportations_unavailable", "Не удалось проверить данные участков перевозок"};
}

}  // namespace shipments
