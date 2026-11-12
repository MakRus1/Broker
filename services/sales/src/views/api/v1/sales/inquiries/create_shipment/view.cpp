#include <components/inquiries.hpp>
#include <components/shipment_route.hpp>
#include <converters/transport_location.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace sales::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    platform::auth::CheckPermission(context, "shipments.write");
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto before = deps.inquiries->FindInquiry(organization, request.body.id);
    if (before.shipment_id || before.converting)
        return Response200{.item = before};

    const auto& token = platform::auth::GetAccessToken(context);
    const auto party =
        deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = before.customer_id}}, token)
            .counterparty;
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    models::ShipmentCreationContext creation{.InquiryRevision = before.revision,
        .Logist = party.logist.value_or(std::nullopt),
        .RouteSegments = before.route_segments};
    if (creation.Logist) {
        const auto employees = deps.organizations.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1, .organization_id = organization, .subject = creation.Logist}},
            token);
        if (employees.items.empty())
            throw ApiError<400>{
                "invalid_logist", "В карточке клиента назначен недействующий логист"};
    }

    const auto references = route_rules::CollectReferences(creation.RouteSegments);
    models::TransportLocations locations;
    if (!references.LocationIds.empty()) {
        const auto response = deps.transport_locations.ApiV1RouteLocationsGetBatch(
            {.body = {.ids = {references.LocationIds.begin(), references.LocationIds.end()}}},
            token);
        for (const auto& location : response.items)
            locations.emplace(location.id, converters::ToTransportLocation(location));
    }
    route_rules::ResolveLocations(creation.RouteSegments, locations);
    for (const auto& id : references.CounterpartyIds) {
        const auto counterparty =
            deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = id}}, token).counterparty;
        if (counterparty.archived)
            throw ApiError<400>{
                "invalid_route_counterparty", "Выберите действующего контрагента участка"};
    }

    return Response200{
        .item = deps.inquiries->StartShipmentCreation(organization, actor, request.body, creation)};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

}  // namespace sales::views::api::v1::sales::inquiries::create_shipment
