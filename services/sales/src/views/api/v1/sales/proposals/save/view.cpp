#include <components/inquiries.hpp>
#include <components/proposal_calculation.hpp>
#include <components/proposals.hpp>
#include <components/rfq_rules.hpp>
#include <converters/inputs.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace sales::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& token = platform::auth::GetAccessToken(context);
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    auto input = converters::ToProposalInput(request.body);
    proposal_rules::ValidateInput(input, inquiry);

    const auto settings = deps.finance.ApiV1FinanceSettingsGet({}, token);
    const auto customer =
        deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = inquiry.customer_id}}, token)
            .counterparty;
    models::ProposalCalculation data{.InquiryRevision = inquiry.revision,
        .CustomerName = customer.name,
        .ReportingCurrency = settings.reporting_currency};
    for (auto& line : input.lines) {
        if (line.quote_source) {
            const auto& source = *line.quote_source;
            const auto rfq =
                deps.rfq->FindRfqRevision(organization, source.rfq_id, source.revision);
            rfq_rules::ApplyQuote(input, inquiry, rfq, line);
        }
        const auto label =
            deps.finance.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token).label;
        if (label.archived ||
            label.direction == clients::finance::models::FinancialLabelDirection::Expense ||
            (line.purchase &&
                label.direction != clients::finance::models::FinancialLabelDirection::Both))
            throw ApiError<400>{
                "invalid_label", "Выберите действующую метку, подходящую для дохода и расхода"};
        data.LabelNames.emplace(line.label_id, label.name);
        if (line.supplier_id) {
            const auto supplier =
                deps.counterparties
                    .ApiV1CounterpartiesGet({.body = {.id = *line.supplier_id}}, token)
                    .counterparty;
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик находится в архиве"};
            data.SupplierNames.emplace(*line.supplier_id, supplier.name);
        }
    }

    std::set<std::string> currencies;
    for (const auto& line : input.lines) {
        currencies.insert(line.sale.currency);
        if (line.purchase)
            currencies.insert(line.purchase->currency);
    }
    for (const auto& currency : currencies) {
        data.Rates[currency] = currency == data.ReportingCurrency
            ? 1000000
            : deps.finance
                  .ApiV1ExchangeRatesGet({.body = {.date = input.calculation_date,
                                              .from_currency = currency,
                                              .to_currency = data.ReportingCurrency}},
                      token)
                  .rate_millionths;
    }
    auto value = proposal_rules::CalculateProposal(std::move(input), data);

    return Response200{.item = deps.proposals->SaveProposal(
                           organization, actor, request.body.expected_revision, std::move(value))};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам расчёта"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить данные для расчёта"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

}  // namespace sales::views::api::v1::sales::proposals::save
