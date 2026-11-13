#include <components/service_charges.hpp>
#include <helpers/document_parties.hpp>
#include <helpers/exchange_amounts.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <utility>
#include <views/api/v1/service_charges/save/view.hpp>

namespace finance::views::api::v1::service_charges::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_SERVICE_CHARGES);

    const models::ServiceChargeScope scope{.shipment_id = request.body.shipment_id,
        .transportation_id = request.body.transportation_id};

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

    const auto& value = request.body;
    const auto& body = request.body;
    models::ServiceChargeParties parties;
    try {
        const auto party =
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = value.counterparty_id}}, token)
                .counterparty;
        if (party.archived && !value.archived)
            throw ApiError<409>{"archived_party", "Выберите действующего контрагента"};
        parties.CounterpartyName = party.name;
        if (value.contract_id.has_value()) {
            const auto contract =
                deps.contracts.ApiV1ContractsGet({.body = {.id = value.contract_id.value()}}, token)
                    .contract;
            if (!helpers::SameId(contract.counterparty_id, value.counterparty_id))
                throw ApiError<400>{"wrong_contract", "Договор относится к другому контрагенту"};
            if (contract.currency != value.invoice_currency)
                throw ApiError<400>{
                    "contract_currency", "Валюта счёта должна совпадать с валютой договора"};
            if (contract.archived && !value.archived)
                throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
            parties.ContractNumber = contract.number;
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

    helpers::ValidateCurrency(body.currency);
    helpers::ValidateCurrency(body.invoice_currency);
    const models::ConversionRate exchange_rate = body.manual_invoice_rate_millionths.has_value()
        ? models::ConversionRate{.Millionths = body.manual_invoice_rate_millionths.value(),
              .Date = body.service_date,
              .Source = "manual"}
        : deps.exchange_rates.GetConversionRate(
              body.currency, body.invoice_currency, body.service_date);
    if (body.currency == body.invoice_currency && exchange_rate.Millionths != 1000000)
        throw ApiError<400>{
            "identity_exchange_rate", "Для одинаковых валют курс должен быть равен 1"};

    return Response200{
        .charge = deps.service_charges->SaveServiceCharge(organization, identity, request.body,
            parties, exchange_rate,
            platform::auth::HasPermission(context, permissions::FINANCE_WRITE_INVOICES))};
}
}  // namespace finance::views::api::v1::service_charges::save
