#pragma once
#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <chrono>
#include <configs/keys.hpp>
#include <deps.hpp>
#include <helpers/converters/route_details.hpp>
#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <openapi/models.hpp>
#include <platform/date_validation.hpp>
#include <postgresql/types.hpp>
#include <set>
#include <userver/utils/datetime.hpp>
#include <userver/utils/text.hpp>

namespace shipments::helpers {
inline auto GetPlatformShipmentSettings(const Deps& deps, const std::string& organization,
    const std::optional<std::string>& customer = std::nullopt) {
    return deps.configs.GetSnapshot().Get(configs::SHIPMENT_SETTINGS,
        config_contexts::ShipmentOrganizationContext{
            .CustomerId = customer, .OrganizationId = organization});
}

inline std::optional<pgtypes::OrganizationShipmentRulesV1> LoadOrganizationRules(
    const Deps& deps, const std::string& organization) {
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOrganizationShipmentRules, organization);
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<pgtypes::OrganizationShipmentRulesV1>();
}

inline auto GetShipmentSettings(const Deps& deps, const std::string& organization,
    const std::optional<std::string>& customer = std::nullopt) {
    auto result = GetPlatformShipmentSettings(deps, organization, customer);
    const auto limits = GetPlatformShipmentSettings(deps, organization);
    const auto stored = LoadOrganizationRules(deps, organization);
    if (stored.has_value()) {
        const auto& rules = stored.value().Rules;
        result.NumberTemplate = rules.NumberTemplate;
        result.ResetNumberYearly = rules.ResetNumberYearly;
        result.Statuses = rules.Statuses;
        result.RouteTypes = rules.RouteTypes;
        result.PlannedContainers = rules.PlannedContainers;
        result.ContainerTypes = rules.ContainerTypes;
    }
    const auto intersect = [](auto& values, const auto& allowed) {
        std::erase_if(values, [&](const auto& value) {
            return std::find(allowed.begin(), allowed.end(), value) == allowed.end();
        });
    };
    intersect(result.RouteTypes, limits.RouteTypes);
    intersect(result.ContainerTypes, limits.ContainerTypes);
    result.PlannedContainers = result.PlannedContainers && limits.PlannedContainers;
    return result;
}

inline std::optional<std::string> CleanText(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto result = userver::utils::text::Trim(value.value());
    return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

inline userver::utils::datetime::Date Today() {
    return userver::utils::datetime::Date{
        std::chrono::floor<std::chrono::days>(userver::utils::datetime::Now())};
}

inline void ValidateShipmentDate(userver::utils::datetime::Date value) {
    using namespace std::chrono;
    if (!platform::IsCalendarDateInRange(
            value, year{1900} / January / 1, year{9999} / December / 31))
        throw ApiError{
            400, "invalid_date", "Укажите корректную дату поставки не раньше 01.01.1900"};
}

template <typename Request> void ApplyCard(const Request& request, pgtypes::ShipmentDataV3& row) {
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
                throw ApiError{
                    400, "duplicate_segment", "Идентификаторы участков должны быть уникальными"};
            row.RouteSegments.push_back(Convert(segment));
        }
    }
    ValidateShipmentDate(row.ShipmentDate);
}

inline void ValidateCard(const pgtypes::ShipmentDataV3& row, const auto& settings,
    const std::vector<pgtypes::ShipmentDictionaryValueV1>& dictionaries,
    const std::optional<pgtypes::ShipmentV4>& previous = std::nullopt) {
    if ((!previous.has_value() || previous.value().Status != row.Status) &&
        std::find(settings.Statuses.begin(), settings.Statuses.end(), row.Status) ==
            settings.Statuses.end())
        throw ApiError{400, "invalid_status", "Статус недоступен для организации"};
    for (const auto& segment : row.RouteSegments) {
        const bool unchanged = previous.has_value() &&
            std::any_of(previous.value().RouteSegments.begin(),
                previous.value().RouteSegments.end(),
                [&](const auto& old) { return old.Id == segment.Id && old.Type == segment.Type; });
        if (!unchanged &&
            (std::find(settings.RouteTypes.begin(), settings.RouteTypes.end(), segment.Type) ==
                    settings.RouteTypes.end() ||
                std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                    return ParseDictionary(entry.Dictionary) == ShipmentDictionary::kRouteType &&
                        entry.Code == segment.Type;
                })))
            throw ApiError{400, "invalid_route_type", "Тип участка недоступен для организации"};
    }
    const auto validate = [&](const std::optional<std::string>& value,
                              ShipmentDictionary dictionary, const std::string& message,
                              const std::optional<std::string>& old) {
        if (!value.has_value() || (previous.has_value() && old == value))
            return;
        if (std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                return ParseDictionary(entry.Dictionary) == dictionary &&
                    entry.Code == value.value();
            }))
            throw ApiError{400, "invalid_" + std::string{DictionaryCode(dictionary)}, message};
    };
    validate(row.Incoterms, ShipmentDictionary::kIncoterms, "Выберите Incoterms из справочника",
        previous.has_value() ? previous.value().Incoterms : std::nullopt);
    validate(row.CarriageTerms, ShipmentDictionary::kCarriageTerms,
        "Выберите условия перевозки из справочника",
        previous.has_value() ? previous.value().CarriageTerms : std::nullopt);
    validate(row.Loading, ShipmentDictionary::kLoading, "Выберите загрузку из справочника",
        previous.has_value() ? previous.value().Loading : std::nullopt);
}

inline std::string GenerateShipmentNumber(pg::Transaction& transaction,
    const std::string& organization, userver::utils::datetime::Date date, const auto& settings) {
    const auto formatted = userver::utils::datetime::ToString(date);
    const std::chrono::year_month_day calendar{
        std::chrono::floor<std::chrono::days>(date.GetSysDays())};
    const auto period = settings.ResetNumberYearly ? static_cast<int>(calendar.year()) : 0;
    const auto sequence = transaction.Execute(sql::kNextShipmentNumber, organization, period)
                              .template AsSingleRow<std::int64_t>();
    std::string result = settings.NumberTemplate;
    const std::vector<std::pair<std::string, std::string>> tokens{
        {"{sequence}", std::to_string(sequence)}, {"{yyyy}", formatted.substr(0, 4)},
        {"{yy}", formatted.substr(2, 2)}, {"{mm}", formatted.substr(5, 2)},
        {"{dd}", formatted.substr(8, 2)}};
    if (result.find("{sequence}") == std::string::npos)
        throw ApiError{400, "invalid_number_template", "Шаблон номера должен содержать {sequence}"};
    for (const auto& [token, value] : tokens) {
        std::size_t position = 0;
        while ((position = result.find(token, position)) != std::string::npos) {
            result.replace(position, token.size(), value);
            position += value.size();
        }
    }
    if (result.empty() || result.size() > 128 || result.find_first_of("{}") != std::string::npos)
        throw ApiError{400, "invalid_number_template", "Некорректный шаблон номера организации"};
    return result;
}

inline void RememberCargo(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& customer, const std::optional<std::string>& description) {
    if (description.has_value())
        transaction.Execute(sql::kRememberCargo, organization, customer, description.value(),
            userver::utils::text::ToLower(description.value()));
}
}  // namespace shipments::helpers
