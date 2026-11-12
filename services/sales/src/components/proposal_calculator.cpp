#include <components/proposal_calculator.hpp>
#include <components/rfq.hpp>
#include <components/sales_directory.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/charge_amounts.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/decimal64/decimal64.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
using sales::ApiError;

models::ProposalAmounts Amounts(std::int64_t quantity, const models::ProposalPrice& value) {
    const auto a = platform::CalculateChargeAmounts(quantity, value.price_minor,
        value.price_mode == models::PriceMode::Gross, value.vat_mode == models::VatMode::Exempt,
        value.vat_rate_basis_points);
    return {.net_minor = a.Net, .total_minor = a.Total, .vat_minor = a.Vat};
}

void AddAmounts(models::ProposalAmounts& target, const models::ProposalAmounts& value) {
    target.net_minor += value.net_minor;
    target.vat_minor += value.vat_minor;
    target.total_minor += value.total_minor;
}

std::int64_t ConvertProposalAmount(std::int64_t amount, std::int64_t rate) {
    using Amount = userver::decimal64::Decimal<0, userver::decimal64::HalfUpRoundPolicy>;
    using Rate = userver::decimal64::Decimal<6, userver::decimal64::HalfUpRoundPolicy>;
    const auto result = (Amount{amount} * Rate::FromUnbiased(rate)).AsUnbiased();
    // Ограничиваем сумму для точного представления целых чисел в JSON у потребителей.
    if (result > 90000000000000LL)
        throw ApiError<400>{"amount_overflow", "Сумма предложения слишком велика"};
    return result;
}

}  // namespace

namespace sales {
namespace json = userver::formats::json;

ProposalCalculator::ProposalCalculator(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      finance_(context.FindComponent<clients::finance::Component>().GetClient()),
      rfq_(context.FindComponent<RfqComponent>()),
      directory_(context.FindComponent<SalesDirectory>()) {}

userver::yaml_config::Schema ProposalCalculator::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::Proposal ProposalCalculator::CalculateProposal(const std::string& organization,
    std::string_view token, const models::Inquiry& inquiry, models::ProposalInput input) const try {
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название предложения"};
    if (input.valid_until && *input.valid_until < input.calculation_date)
        throw ApiError<400>{"invalid_dates", "Срок действия не может быть раньше даты расчёта"};
    const auto settings = finance_.ApiV1FinanceSettingsGet({}, token);
    models::Proposal result;
    if (!inquiry.cargo_description ||
        userver::utils::text::Trim(*inquiry.cargo_description).empty())
        throw ApiError<400>{
            "cargo_required", "Перед расчётом предложения опишите груз во входящем запросе"};
    result.inquiry_revision = inquiry.revision;
    result.customer_name = directory_.GetCounterparty(token, inquiry.customer_id).name;
    result.input = std::move(input);
    result.status = models::ProposalStatus::Draft;
    result.reporting_currency = settings.reporting_currency;
    result.reporting.currency = result.reporting_currency;
    std::map<std::string, models::ProposalTotal> totals;
    std::map<std::string, std::int64_t> rates;
    std::set<std::string> ids;
    const auto accumulate = [&](const models::ProposalPrice& price,
                                const models::ProposalAmounts& amount, bool sale) {
        auto& total = totals[price.currency];
        total.currency = price.currency;
        AddAmounts(sale ? total.sale : total.purchase, amount);
        if (!rates.contains(price.currency)) {
            rates[price.currency] = price.currency == result.reporting_currency
                ? 1000000
                : finance_
                      .ApiV1ExchangeRatesGet({.body = {.date = result.input.calculation_date,
                                                  .from_currency = price.currency,
                                                  .to_currency = result.reporting_currency}},
                          token)
                      .rate_millionths;
        }
        const auto rate = rates.at(price.currency);
        const auto net = ConvertProposalAmount(amount.net_minor, rate);
        const auto gross = ConvertProposalAmount(amount.total_minor, rate);
        AddAmounts(sale ? result.reporting.sale : result.reporting.purchase,
            {.net_minor = net, .total_minor = gross, .vat_minor = gross - net});
    };
    for (auto& line : result.input.lines) {
        rfq_.ResolveQuote(organization, result.input, inquiry, line);
        if (!ids.insert(line.id).second)
            throw ApiError<400>{
                "duplicate_line", "Строки предложения должны иметь разные идентификаторы"};
        const auto label =
            finance_.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token).label;
        if (label.archived ||
            label.direction == clients::finance::models::FinancialLabelDirection::Expense ||
            (line.purchase &&
                label.direction != clients::finance::models::FinancialLabelDirection::Both))
            throw ApiError<400>{
                "invalid_label", "Выберите действующую метку, подходящую для дохода и расхода"};
        models::CalculatedProposalLine row;
        row.input = line;
        row.label_name = label.name;
        row.sale = Amounts(line.quantity_milli, line.sale);
        accumulate(line.sale, row.sale, true);
        if (line.purchase) {
            row.purchase = Amounts(line.quantity_milli, *line.purchase);
            accumulate(*line.purchase, *row.purchase, false);
        }
        if (line.supplier_id) {
            const auto party = directory_.GetCounterparty(token, *line.supplier_id);
            if (party.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик находится в архиве"};
            row.supplier_name = party.name;
        }
        result.lines.push_back(std::move(row));
    }
    for (const auto& [_, total] : totals)
        result.totals.push_back(total);
    for (const auto& [currency, rate] : rates)
        result.rates.push_back({.currency = currency, .rate_millionths = rate});
    if (result.reporting.sale.total_minor > 9000000000000000LL ||
        result.reporting.purchase.total_minor > 9000000000000000LL)
        throw ApiError<400>{"amount_overflow", "Сумма предложения слишком велика"};
    return result;
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

}  // namespace sales
