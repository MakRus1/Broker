#include <algorithm>
#include <components/rfq_rules.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/charge_amounts.hpp>
#include <set>
#include <userver/formats/json.hpp>
#include <userver/utils/text_light.hpp>

namespace {
struct PurchaseOption {
    sales::models::ProposalQuoteSource Source;
    const sales::models::RfqQuoteLine* Line;
    std::int64_t Net;
};

struct ServicePurchase {
    std::string Name;
    std::optional<PurchaseOption> Cheapest;
    bool DifferentVolume = false;
    bool DifferentCurrency = false;
};
}  // namespace

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

models::ProposalSuggestionResult SuggestMinimumPurchase(const models::Inquiry& inquiry,
    const std::vector<models::Rfq>& items, userver::utils::datetime::Date calculation_date) {
    std::map<boost::uuids::uuid, std::map<boost::uuids::uuid, ServicePurchase>>
        purchases_by_segment;
    models::ProposalSuggestionResult result;

    for (const auto& rfq : items) {
        if (rfq.input.status != models::RfqStatus::Ready)
            continue;
        const auto segment =
            std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
                [&](const auto& value) { return value.id == rfq.input.route_segment_id; });
        if (segment == inquiry.route_segments.end() ||
            json::ValueBuilder(*segment).ExtractValue() !=
                json::ValueBuilder(rfq.route_segment).ExtractValue())
            continue;
        result.items.push_back(rfq);

        for (const auto& quote : rfq.input.quotes) {
            const bool expired = quote.valid_until && *quote.valid_until < calculation_date;
            for (const auto& line : quote.lines) {
                auto& services = purchases_by_segment[rfq.input.route_segment_id];
                auto [position, inserted] = services.try_emplace(line.label_id);
                auto& purchase = position->second;
                if (inserted)
                    purchase.Name = line.label_name.value_or("");
                if (expired)
                    continue;

                if (purchase.Cheapest) {
                    const auto& previous = *purchase.Cheapest->Line;
                    if (previous.unit != line.unit ||
                        previous.quantity_milli != line.quantity_milli) {
                        purchase.DifferentVolume = true;
                        continue;
                    }
                    if (previous.price.currency != line.price.currency) {
                        purchase.DifferentCurrency = true;
                        continue;
                    }
                }
                const auto amounts = platform::CalculateChargeAmounts(line.quantity_milli,
                    line.price.price_minor, line.price.price_mode == models::PriceMode::Gross,
                    line.price.vat_mode == models::VatMode::Exempt,
                    line.price.vat_rate_basis_points);
                if (!purchase.Cheapest || amounts.Net < purchase.Cheapest->Net)
                    purchase.Cheapest = PurchaseOption{.Source = {.line_id = line.id,
                                                           .quote_id = quote.id,
                                                           .revision = rfq.revision,
                                                           .rfq_id = rfq.input.id},
                        .Line = &line,
                        .Net = amounts.Net};
            }
        }
    }

    for (const auto& segment : inquiry.route_segments) {
        const auto services = purchases_by_segment.find(segment.id);
        if (services == purchases_by_segment.end()) {
            result.gaps.push_back({.name = "",
                .reason = models::ProposalSuggestionGapReason::Missing,
                .segment = segment.id});
            continue;
        }
        for (const auto& [label, purchase] : services->second) {
            std::optional<models::ProposalSuggestionGapReason> reason;
            if (!purchase.Cheapest)
                reason = models::ProposalSuggestionGapReason::Expired;
            else if (purchase.DifferentVolume)
                reason = models::ProposalSuggestionGapReason::Volume;
            else if (purchase.DifferentCurrency)
                reason = models::ProposalSuggestionGapReason::Currency;

            if (reason)
                result.gaps.push_back(
                    {.name = purchase.Name, .reason = *reason, .segment = segment.id});
            else
                result.selected.push_back(purchase.Cheapest->Source);
        }
    }
    return result;
}

}  // namespace sales::rfq_rules
