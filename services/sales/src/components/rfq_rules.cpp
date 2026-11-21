#include <algorithm>
#include <components/rfq_rules.hpp>
#include <map>
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
    std::set<boost::uuids::uuid> quote_ids;
    for (const auto& quote : input.quotes) {
        if (!quote_ids.insert(quote.id).second)
            throw ApiError<400>{"duplicate_quote", "У котировок должны быть разные идентификаторы"};
        if (userver::utils::text::Trim(quote.title).empty())
            throw ApiError<400>{"empty_quote_title", "Укажите название варианта поставщика"};
        std::set<boost::uuids::uuid> lines;
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

models::MinimumPurchaseResult SuggestMinimumPurchase(const models::Inquiry& inquiry,
    const std::vector<models::Rfq>& items, userver::utils::datetime::Date calculation_date) {
    struct Candidate {
        const models::Rfq* Rfq;
        const models::RfqQuote* Quote;
        const models::RfqQuoteLine* Line;
        std::int64_t Net;
    };

    using Key = std::pair<boost::uuids::uuid, boost::uuids::uuid>;
    std::map<Key, std::vector<Candidate>> services;
    models::MinimumPurchaseResult result;
    for (const auto& rfq : items) {
        if (rfq.input.status != models::RfqStatus::Ready)
            continue;
        if (std::none_of(inquiry.route_segments.begin(), inquiry.route_segments.end(),
                [&](const auto& segment) {
                    return segment.id == rfq.input.route_segment_id &&
                        json::ValueBuilder(segment).ExtractValue() ==
                        json::ValueBuilder(rfq.route_segment).ExtractValue();
                }))
            continue;
        result.items.push_back(rfq);
        for (const auto& quote : rfq.input.quotes)
            for (const auto& line : quote.lines) {
                const auto amounts = platform::CalculateChargeAmounts(line.quantity_milli,
                    line.price.price_minor, line.price.price_mode == models::PriceMode::Gross,
                    line.price.vat_mode == models::VatMode::Exempt,
                    line.price.vat_rate_basis_points);
                services[{rfq.input.route_segment_id, line.label_id}].push_back(
                    {&rfq, &quote, &line, amounts.Net});
            }
    }

    for (const auto& [key, candidates] : services) {
        std::vector<const Candidate*> available;
        for (const auto& candidate : candidates)
            if (!candidate.Quote->valid_until || *candidate.Quote->valid_until >= calculation_date)
                available.push_back(&candidate);
        std::optional<models::MinimumPurchaseGapReason> reason;
        if (available.empty())
            reason = models::MinimumPurchaseGapReason::Expired;
        else {
            const auto& first = *available.front()->Line;
            if (std::any_of(available.begin(), available.end(), [&](const auto* candidate) {
                    return candidate->Line->unit != first.unit ||
                        candidate->Line->quantity_milli != first.quantity_milli;
                }))
                reason = models::MinimumPurchaseGapReason::Volume;
            else if (std::any_of(available.begin(), available.end(), [&](const auto* candidate) {
                         return candidate->Line->price.currency != first.price.currency;
                     }))
                reason = models::MinimumPurchaseGapReason::Currency;
        }
        if (reason) {
            const auto& line = *candidates.front().Line;
            result.gaps.push_back(
                {.name = line.label_name.value_or(""), .reason = *reason, .segment = key.first});
            continue;
        }
        const auto* best = *std::min_element(available.begin(), available.end(),
            [](const auto* a, const auto* b) { return a->Net < b->Net; });
        result.selected.push_back({.line_id = best->Line->id,
            .quote_id = best->Quote->id,
            .revision = best->Rfq->revision,
            .rfq_id = best->Rfq->input.id});
    }
    for (const auto& segment : inquiry.route_segments)
        if (std::none_of(services.begin(), services.end(),
                [&](const auto& service) { return service.first.first == segment.id; }))
            result.gaps.push_back({.name = "",
                .reason = models::MinimumPurchaseGapReason::Missing,
                .segment = segment.id});
    return result;
}

}  // namespace sales::rfq_rules
