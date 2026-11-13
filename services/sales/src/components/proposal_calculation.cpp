#include <components/proposal_calculation.hpp>
#include <models/errors.hpp>
#include <platform/charge_amounts.hpp>
#include <set>
#include <userver/decimal64/decimal64.hpp>
#include <userver/utils/text_light.hpp>

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

namespace sales::proposal_rules {
void ValidateInput(const models::ProposalInput& input, const models::Inquiry& inquiry) {
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название предложения"};
    if (input.valid_until && *input.valid_until < input.calculation_date)
        throw ApiError<400>{"invalid_dates", "Срок действия не может быть раньше даты расчёта"};
    if (!inquiry.cargo_description ||
        userver::utils::text::Trim(*inquiry.cargo_description).empty())
        throw ApiError<400>{
            "cargo_required", "Перед расчётом предложения опишите груз во входящем запросе"};
}

models::Proposal CalculateProposal(
    models::ProposalInput input, const models::ProposalCalculation& data) {
    models::Proposal result;
    result.inquiry_revision = data.InquiryRevision;
    result.customer_name = data.CustomerName;
    result.input = std::move(input);
    result.status = models::ProposalStatus::Draft;
    result.reporting_currency = data.ReportingCurrency;
    result.reporting.currency = result.reporting_currency;
    std::map<std::string, models::ProposalTotal> totals;
    std::map<std::string, std::int64_t> rates;
    std::set<std::string> ids;
    const auto accumulate = [&](const models::ProposalPrice& price,
                                const models::ProposalAmounts& amount, bool sale) {
        auto& total = totals[price.currency];
        total.currency = price.currency;
        AddAmounts(sale ? total.sale : total.purchase, amount);
        rates[price.currency] = data.Rates.at(price.currency);
        const auto rate = rates.at(price.currency);
        const auto net = ConvertProposalAmount(amount.net_minor, rate);
        const auto gross = ConvertProposalAmount(amount.total_minor, rate);
        AddAmounts(sale ? result.reporting.sale : result.reporting.purchase,
            {.net_minor = net, .total_minor = gross, .vat_minor = gross - net});
    };
    for (auto& line : result.input.lines) {
        if (!ids.insert(line.id).second)
            throw ApiError<400>{
                "duplicate_line", "Строки предложения должны иметь разные идентификаторы"};
        models::CalculatedProposalLine row;
        row.input = line;
        row.label_name = data.LabelNames.at(line.label_id);
        row.sale = Amounts(line.quantity_milli, line.sale);
        accumulate(line.sale, row.sale, true);
        if (line.purchase) {
            row.purchase = Amounts(line.quantity_milli, *line.purchase);
            accumulate(*line.purchase, *row.purchase, false);
        }
        if (line.supplier_id)
            row.supplier_name = data.SupplierNames.at(*line.supplier_id);
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
}
}  // namespace sales::proposal_rules
