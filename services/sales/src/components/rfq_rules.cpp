#include <algorithm>
#include <components/rfq_rules.hpp>
#include <models/errors.hpp>
#include <platform/charge_amounts.hpp>
#include <set>
#include <userver/formats/json.hpp>
#include <userver/utils/text_light.hpp>

namespace sales::rfq_rules {
namespace json = userver::formats::json;

void ValidateInput(const models::RfqInput& input) {
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название запроса ставок"};
    if (input.status == models::RfqStatus::Ready && input.quotes.empty())
        throw ApiError<400>{"quotes_required", "Добавьте хотя бы одну котировку"};
    std::set<std::string> quote_ids;
    for (const auto& quote : input.quotes) {
        if (!quote_ids.insert(quote.id).second)
            throw ApiError<400>{"duplicate_quote", "У котировок должны быть разные идентификаторы"};
        if (userver::utils::text::Trim(quote.title).empty())
            throw ApiError<400>{"empty_quote_title", "Укажите название варианта поставщика"};
        std::set<std::string> lines;
        for (const auto& line : quote.lines) {
            if (!lines.insert(line.id).second)
                throw ApiError<400>{"duplicate_line", "У строк должны быть разные идентификаторы"};
            platform::CalculateChargeAmounts(line.quantity_milli, line.price.price_minor,
                line.price.price_mode == models::PriceMode::Gross,
                line.price.vat_mode == models::VatMode::Exempt, line.price.vat_rate_basis_points);
        }
    }
}

void ApplyQuote(const models::ProposalInput& proposal, const models::Inquiry& inquiry,
    const models::Rfq& value, models::ProposalLine& line) {
    if (!line.quote_source)
        return;
    const auto& source = *line.quote_source;
    if (value.input.inquiry_id != proposal.inquiry_id ||
        value.input.status != models::RfqStatus::Ready)
        throw ApiError<400>{"wrong_quote_source", "Выберите готовую котировку этого запроса"};
    const auto segment = std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
        [&](const auto& candidate) { return candidate.id == value.input.route_segment_id; });
    if (segment == inquiry.route_segments.end() ||
        json::ValueBuilder(*segment).ExtractValue() !=
            json::ValueBuilder(value.route_segment).ExtractValue())
        throw ApiError<400>{
            "quote_route_changed", "Участок изменился. Уточните котировку у поставщика"};

    for (const auto& quote : value.input.quotes) {
        if (quote.id != source.quote_id)
            continue;
        if (quote.valid_until && *quote.valid_until < proposal.calculation_date)
            throw ApiError<400>{"quote_expired", "Срок действия котировки истёк"};
        for (const auto& quoted : quote.lines) {
            if (quoted.id != source.line_id)
                continue;
            line.label_id = quoted.label_id;
            line.unit = quoted.unit;
            line.quantity_milli = quoted.quantity_milli;
            line.purchase = quoted.price;
            line.supplier_id = quote.supplier_id;
            line.route_segment_id = value.input.route_segment_id;
            return;
        }
    }
    throw ApiError<400>{"quote_not_found", "Строка котировки не найдена"};
}

}  // namespace sales::rfq_rules
