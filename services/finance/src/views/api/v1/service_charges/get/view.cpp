#include <components/service_charges.hpp>
#include <helpers/document_parties.hpp>
#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <utility>
#include <views/api/v1/service_charges/get/view.hpp>

namespace finance::views::api::v1::service_charges::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);

    const auto charge = deps.service_charges->FindServiceCharge(organization, request.body);
    const models::ServiceChargeScope scope{
        .shipment_id = charge.shipment_id, .transportation_id = charge.transportation_id};

    if (scope.transportation_id.has_value() && !scope.shipment_id.has_value())
        throw ApiError<400>{"shipment_required", "Для услуги перевозки нужна поставка"};
    const auto token = platform::auth::GetAccessToken(context);
    try {
        if (scope.transportation_id.has_value()) {
            const auto transportation =
                deps.transportations
                    .ApiV1TransportationsGet(
                        {.body = {.transportation_id = scope.transportation_id.value()}}, token)
                    .transportation;
            if (!helpers::SameId(transportation.shipment_id, scope.shipment_id.value()))
                throw ApiError<400>{"wrong_shipment", "Перевозка относится к другой поставке"};
        } else if (scope.shipment_id.has_value()) {
            deps.shipments.ApiV1ShipmentsGet(
                {.body = {.shipment_id = scope.shipment_id.value()}}, token);
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к связанным данным услуги"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "reference_unavailable", "Не удалось проверить связанные данные услуги"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }

    return Response200{.charge = charge};
}
}  // namespace finance::views::api::v1::service_charges::get
