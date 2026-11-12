#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <chrono>
#include <converters/shipments.hpp>
#include <domain/shipment_card.hpp>
#include <helpers/dictionaries.hpp>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <set>
#include <userver/utils/datetime.hpp>
#include <userver/utils/text.hpp>

namespace shipments::domain {
namespace {
userver::utils::datetime::Date Today() {
    return userver::utils::datetime::Date{
        std::chrono::floor<std::chrono::days>(userver::utils::datetime::Now())};
}

void ValidateShipmentDate(userver::utils::datetime::Date value) {
    if (!platform::IsCalendarDateInRange(value, std::chrono::year{1900} / std::chrono::January / 1,
            std::chrono::year{9999} / std::chrono::December / 31))
        throw ApiError<400>{
            "invalid_date", "Укажите корректную дату поставки не раньше 01.01.1900"};
}

template <typename Request> void ApplyCard(const Request& request, models::pg::ShipmentData& row) {
    if (request.shipment_date.has_value())
        row.ShipmentDate = request.shipment_date.value();
    if (request.recipient_id.has_value()) {
        const auto value = request.recipient_id.value();
        row.RecipientId = value.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.value())}
            : std::nullopt;
    }
    if (request.logist.has_value())
        row.Logist = CleanText(request.logist.value());
    if (request.cargo_description.has_value())
        row.CargoDescription = CleanText(request.cargo_description.value());
    if (request.status.has_value())
        row.Status = request.status.value();
    if (request.incoterms.has_value())
        row.Incoterms = CleanText(request.incoterms.value());
    if (request.carriage_terms.has_value())
        row.CarriageTerms = CleanText(request.carriage_terms.value());
    if (request.loading.has_value())
        row.Loading = CleanText(request.loading.value());
    if (request.delivery_city.has_value())
        row.DeliveryCity = CleanText(request.delivery_city.value());
    if (request.delivery_country.has_value())
        row.DeliveryCountry = request.delivery_country.value();
    if (request.delivery_address.has_value())
        row.DeliveryAddress = CleanText(request.delivery_address.value());
    if (request.route_segments.has_value()) {
        row.RouteSegments.clear();
        std::set<std::string> ids;
        for (const auto& segment : request.route_segments.value()) {
            if (!ids.insert(segment.id).second)
                throw ApiError<400>{
                    "duplicate_segment", "Идентификаторы участков должны быть уникальными"};
            row.RouteSegments.push_back(converters::Convert(segment));
        }
    }
    ValidateShipmentDate(row.ShipmentDate);
}

}  // namespace

std::optional<std::string> CleanText(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto result = userver::utils::text::Trim(value.value());
    return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

models::pg::ShipmentData CreateShipmentCard(const models::CreateShipmentRequest& request,
    const configs::detail_SHIPMENT_SETTINGS::Value& settings) {
    models::pg::ShipmentData card;
    card.CustomerId = boost::uuids::string_generator{}(request.customer_id);
    card.Reference = request.reference.value_or("");
    card.ShipmentDate = Today();
    card.Status = settings.Statuses.front();
    card.Logist = request.logist.value_or(std::nullopt);
    ApplyCard(request, card);
    return card;
}

models::pg::ShipmentData UpdateShipmentCard(
    const models::UpdateShipmentRequest& request, const models::pg::ShipmentRaw& previous) {
    auto card = converters::ConvertData(previous);
    if (request.reference.has_value())
        card.Reference = request.reference.value();
    if (request.customer_id.has_value())
        card.CustomerId = boost::uuids::string_generator{}(request.customer_id.value());
    if (request.number.has_value()) {
        card.Number = userver::utils::text::Trim(request.number.value());
        if (card.Number.empty())
            throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
    }
    ApplyCard(request, card);
    return card;
}

void ValidateCard(const models::pg::ShipmentData& row,
    const configs::detail_SHIPMENT_SETTINGS::Value& settings,
    const std::vector<models::pg::ShipmentDictionaryValue>& dictionaries,
    const models::pg::ShipmentRaw* previous) {
    if ((previous == nullptr || previous->Status != row.Status) &&
        std::find(settings.Statuses.begin(), settings.Statuses.end(), row.Status) ==
            settings.Statuses.end())
        throw ApiError<400>{"invalid_status", "Статус недоступен для организации"};
    for (const auto& segment : row.RouteSegments) {
        const bool unchanged = previous != nullptr &&
            std::any_of(previous->RouteSegments.begin(), previous->RouteSegments.end(),
                [&](const auto& old) { return old.Id == segment.Id && old.Type == segment.Type; });
        if (!unchanged &&
            (std::find(settings.RouteTypes.begin(), settings.RouteTypes.end(), segment.Type) ==
                    settings.RouteTypes.end() ||
                std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                    return helpers::ParseDictionary(entry.Dictionary) ==
                        helpers::ShipmentDictionary::kRouteType &&
                        entry.Code == segment.Type;
                })))
            throw ApiError<400>{"invalid_route_type", "Тип участка недоступен для организации"};
    }
    const auto validate = [&](const std::optional<std::string>& value,
                              helpers::ShipmentDictionary dictionary, const std::string& message,
                              const std::optional<std::string>& old) {
        if (!value.has_value() || (previous != nullptr && old == value))
            return;
        if (std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                return helpers::ParseDictionary(entry.Dictionary) == dictionary &&
                    entry.Code == value.value();
            }))
            throw ApiError<400>{
                "invalid_" + std::string{helpers::DictionaryCode(dictionary)}, message};
    };
    validate(row.Incoterms, helpers::ShipmentDictionary::kIncoterms,
        "Выберите Incoterms из справочника",
        previous != nullptr ? previous->Incoterms : std::nullopt);
    validate(row.CarriageTerms, helpers::ShipmentDictionary::kCarriageTerms,
        "Выберите условия перевозки из справочника",
        previous != nullptr ? previous->CarriageTerms : std::nullopt);
    validate(row.Loading, helpers::ShipmentDictionary::kLoading, "Выберите загрузку из справочника",
        previous != nullptr ? previous->Loading : std::nullopt);
}

std::vector<std::string> ChangedRouteSegments(const models::pg::ShipmentRaw& previous,
    const std::vector<models::ShipmentRouteSegment>& next) {
    std::vector<std::string> changed;
    for (const auto& stage : previous.RouteSegments) {
        const auto id = boost::uuids::to_string(stage.Id);
        const auto found =
            std::find_if(next.begin(), next.end(), [&](const auto& item) { return item.id == id; });
        if (found == next.end() || found->type != stage.Type)
            changed.push_back(id);
    }
    return changed;
}

void ValidateRouteUsage(const models::pg::ShipmentRaw& previous,
    const models::pg::ShipmentData& next, const std::vector<std::string>& protected_ids) {
    for (const auto& id : protected_ids) {
        const auto before =
            std::find_if(previous.RouteSegments.begin(), previous.RouteSegments.end(),
                [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
        const auto after = std::find_if(next.RouteSegments.begin(), next.RouteSegments.end(),
            [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
        if (before != previous.RouteSegments.end() &&
            (after == next.RouteSegments.end() || after->Type != before->Type))
            throw ApiError<409>{"route_segment_in_use",
                "Нельзя удалить или изменить тип участка с данными перевозок"};
    }
}
}  // namespace shipments::domain
