#include <components/inquiries.hpp>
#include <components/rfq.hpp>
#include <components/rfq_rules.hpp>
#include <converters/inputs.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <views/api/v1/sales/rfqs/save/view.hpp>

namespace sales::views::api::v1::sales::rfqs::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    auto input = converters::ToRfqInput(request.body);
    rfq_rules::ValidateInput(input);

    const auto party =
        deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = inquiry.customer_id}}, token)
            .counterparty;
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees = deps.organizations.ApiOrganizationsEmployeesList(
        {.body = {.limit = 1,
             .organization_id = organization,
             .subject = input.responsible_subject}},
        token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};

    std::map<std::string, std::string> suppliers;
    std::map<std::string, std::string> labels;
    for (auto& quote : input.quotes) {
        if (!suppliers.contains(quote.supplier_id)) {
            const auto supplier =
                deps.counterparties
                    .ApiV1CounterpartiesGet({.body = {.id = quote.supplier_id}}, token)
                    .counterparty;
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик в архиве"};
            suppliers.emplace(quote.supplier_id, supplier.name);
        }
        quote.supplier_name = suppliers.at(quote.supplier_id);
        for (auto& line : quote.lines) {
            if (!labels.contains(line.label_id)) {
                const auto label =
                    deps.finance.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token)
                        .label;
                if (label.archived ||
                    label.direction == clients::finance::models::FinancialLabelDirection::Income)
                    throw ApiError<400>{"invalid_label", "Выберите метку для расхода"};
                labels.emplace(line.label_id, label.name);
            }
            line.label_name = labels.at(line.label_id);
        }
    }

    return Response200{.item = deps.rfq->SaveRfq(
                           organization, actor, request.body.expected_revision, std::move(input))};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам котировок"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить справочники котировок"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

}  // namespace sales::views::api::v1::sales::rfqs::save
