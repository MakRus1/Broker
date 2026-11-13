#include <components/inquiries.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <views/api/v1/sales/inquiries/save/view.hpp>

namespace sales::views::api::v1::sales::inquiries::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& token = platform::auth::GetAccessToken(context);

    const auto party =
        deps.counterparties
            .ApiV1CounterpartiesGet({.body = {.id = request.body.customer_id}}, token)
            .counterparty;
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees = deps.organizations.ApiOrganizationsEmployeesList(
        {.body = {.limit = 1,
             .organization_id = organization,
             .subject = request.body.manager_subject}},
        token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};

    return Response200{.item = deps.inquiries->SaveInquiry(organization, actor, request.body)};
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

}  // namespace sales::views::api::v1::sales::inquiries::save
