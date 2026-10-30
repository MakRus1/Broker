#pragma once
#include <helpers/shipment_card.hpp>

namespace shipments::helpers {
inline pgtypes::ShipmentBusinessRulesV1 Convert(const models::ShipmentBusinessRules& value) {
    return {.NumberTemplate = value.number_template,
        .ResetNumberYearly = value.reset_number_yearly,
        .Statuses = value.statuses,
        .RouteTypes = value.route_types,
        .PlannedContainers = value.planned_containers,
        .ContainerTypes = value.container_types};
}

inline models::ShipmentBusinessRules Convert(const pgtypes::ShipmentBusinessRulesV1& value) {
    return {.container_types = value.ContainerTypes,
        .number_template = value.NumberTemplate,
        .planned_containers = value.PlannedContainers,
        .reset_number_yearly = value.ResetNumberYearly,
        .route_types = value.RouteTypes,
        .statuses = value.Statuses};
}

inline void ValidateNumberTemplate(const std::string& value) {
    if (value.empty() || value.size() > 128 || value.find("{sequence}") == std::string::npos)
        throw ApiError<400>{"invalid_number_template",
            "Шаблон должен содержать {sequence} и быть не длиннее 128 символов"};
    auto rest = value;
    for (const auto token : {"{sequence}", "{yyyy}", "{yy}", "{mm}", "{dd}"}) {
        std::size_t at;
        while ((at = rest.find(token)) != std::string::npos)
            rest.erase(at, std::string_view{token}.size());
    }
    if (rest.find_first_of("{}") != std::string::npos)
        throw ApiError<400>{"invalid_number_template", "В шаблоне номера неизвестная переменная"};
}

inline void ValidateBusinessRules(const Deps& deps, const std::string& organization,
    const pgtypes::ShipmentBusinessRulesV1& rules) {
    ValidateNumberTemplate(rules.NumberTemplate);
    const auto limits = GetPlatformShipmentSettings(deps, organization);
    const auto unique = [](const auto& values) {
        std::set<std::string> seen;
        for (const auto& value : values)
            if (userver::utils::text::Trim(value).empty() || !seen.insert(value).second)
                throw ApiError<400>{
                    "invalid_rules", "Значения не должны быть пустыми или повторяться"};
    };
    unique(rules.Statuses);
    unique(rules.RouteTypes);
    unique(rules.ContainerTypes);
    if (rules.Statuses.empty())
        throw ApiError<400>{"invalid_statuses", "Нужен хотя бы один статус поставки"};
    const auto subset = [](const auto& values, const auto& allowed) {
        return std::all_of(values.begin(), values.end(), [&](const auto& value) {
            return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
        });
    };
    if (!subset(rules.RouteTypes, limits.RouteTypes) ||
        !subset(rules.ContainerTypes, limits.ContainerTypes) ||
        (rules.PlannedContainers && !limits.PlannedContainers))
        throw ApiError<400>{
            "platform_limit", "Настройки выходят за доступные организации возможности"};
}

inline models::ShipmentBusinessSettings GetBusinessSettings(
    const Deps& deps, const std::string& organization) {
    const auto limits = GetPlatformShipmentSettings(deps, organization);
    const auto effective = GetShipmentSettings(deps, organization);
    const auto stored = LoadOrganizationRules(deps, organization);
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
    models::ShipmentBusinessSettings result;
    result.revision = stored.has_value() ? stored.value().Revision : 0;
    result.rules =
        Convert(pgtypes::ShipmentBusinessRulesV1{.NumberTemplate = effective.NumberTemplate,
            .ResetNumberYearly = effective.ResetNumberYearly,
            .Statuses = effective.Statuses,
            .RouteTypes = effective.RouteTypes,
            .PlannedContainers = effective.PlannedContainers,
            .ContainerTypes = effective.ContainerTypes});
    result.allowed_container_types = limits.ContainerTypes;
    result.planned_containers_allowed = limits.PlannedContainers;
    for (const auto& entry : rows.AsSetOf<pgtypes::ShipmentDictionaryValueV1>())
        if (ParseDictionary(entry.Dictionary) == ShipmentDictionary::kRouteType &&
            std::find(limits.RouteTypes.begin(), limits.RouteTypes.end(), entry.Code) !=
                limits.RouteTypes.end())
            result.allowed_route_types.push_back({.code = entry.Code, .name = entry.Name});
    return result;
}
}  // namespace shipments::helpers
