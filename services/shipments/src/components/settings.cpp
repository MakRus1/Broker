#include <algorithm>
#include <components/settings.hpp>
#include <converters/business_rules.hpp>
#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <models/pg/organization_shipment_rules.hpp>
#include <models/pg/shipment_business_rules.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/text.hpp>

namespace {
namespace models = shipments::models;
namespace pg = userver::storages::postgres;
using shipments::ApiError;

configs::ShipmentSettings ReadPlatformSettings(const platform::configs::Snapshot& snapshot,
    const boost::uuids::uuid& organization,
    const std::optional<boost::uuids::uuid>& customer = std::nullopt) {
    return snapshot.Get(configs::SHIPMENT_SETTINGS,
        config_contexts::ShipmentOrganizationContext{
            .CustomerId = customer, .OrganizationId = organization});
}

configs::ShipmentSettings ResolveShipmentSettings(configs::ShipmentSettings result,
    const configs::ShipmentSettings& limits,
    const std::optional<models::pg::OrganizationShipmentRules>& stored) {
    if (stored.has_value()) {
        const auto& rules = stored.value().Rules;
        result.NumberTemplate = rules.NumberTemplate;
        result.ResetNumberYearly = rules.ResetNumberYearly;
        result.Statuses = rules.Statuses;
        result.RouteTypes = rules.RouteTypes;
        result.PlannedContainers = rules.PlannedContainers;
    }
    const auto intersect = [](auto& values, const auto& allowed) {
        std::erase_if(values, [&](const auto& value) {
            return std::find(allowed.begin(), allowed.end(), value) == allowed.end();
        });
    };
    intersect(result.RouteTypes, limits.RouteTypes);
    result.PlannedContainers = result.PlannedContainers && limits.PlannedContainers;
    return result;
}

void ValidateBusinessRules(
    const configs::ShipmentSettings& limits, const models::pg::ShipmentBusinessRules& rules) {
    platform::ValidateNumberTemplate(rules.NumberTemplate);
    const auto unique = [](const auto& values) {
        std::set<std::string> seen;
        for (const auto& value : values)
            if (userver::utils::text::Trim(value).empty() || !seen.insert(value).second)
                throw ApiError<400>{
                    "invalid_rules", "Значения не должны быть пустыми или повторяться"};
    };
    unique(rules.Statuses);
    unique(rules.RouteTypes);
    if (rules.Statuses.empty())
        throw ApiError<400>{"invalid_statuses", "Нужен хотя бы один статус поставки"};
    const auto subset = [](const auto& values, const auto& allowed) {
        return std::all_of(values.begin(), values.end(), [&](const auto& value) {
            return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
        });
    };
    if (!subset(rules.RouteTypes, limits.RouteTypes) ||
        (rules.PlannedContainers && !limits.PlannedContainers))
        throw ApiError<400>{
            "platform_limit", "Настройки выходят за доступные организации возможности"};
}

std::optional<models::pg::OrganizationShipmentRules> ReadStoredRules(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<models::pg::OrganizationShipmentRules>();
}

configs::ShipmentSettings ResolveEffectiveSettings(const platform::configs::Snapshot& snapshot,
    const boost::uuids::uuid& organization, const std::optional<boost::uuids::uuid>& customer,
    const std::optional<models::pg::OrganizationShipmentRules>& stored) {
    const auto limits = ReadPlatformSettings(snapshot, organization);
    auto defaults =
        customer.has_value() ? ReadPlatformSettings(snapshot, organization, customer) : limits;
    return ResolveShipmentSettings(std::move(defaults), limits, stored);
}

}  // namespace

namespace shipments {
namespace pg = userver::storages::postgres;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      configs_(context.FindComponent<userver::components::DynamicConfig>().GetSource()) {}

std::optional<models::pg::OrganizationShipmentRules> SettingsComponent::LoadRules(
    const boost::uuids::uuid& organization) const {
    return ReadStoredRules(cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOrganizationShipmentRules, organization));
}

configs::ShipmentSettings SettingsComponent::Effective(const boost::uuids::uuid& organization,
    const std::optional<boost::uuids::uuid>& customer) const {
    return ResolveEffectiveSettings(
        configs_.GetSnapshot(), organization, customer, LoadRules(organization));
}

configs::ShipmentSettings SettingsComponent::Effective(pg::Transaction& transaction,
    const boost::uuids::uuid& organization,
    const std::optional<boost::uuids::uuid>& customer) const {
    return ResolveEffectiveSettings(configs_.GetSnapshot(), organization, customer,
        ReadStoredRules(transaction.Execute(sql::kGetOrganizationShipmentRules, organization)));
}

models::ShipmentBusinessSettings SettingsComponent::BuildBusinessSettings(
    const configs::ShipmentSettings& limits, models::ShipmentBusinessRules rules,
    std::int64_t revision) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
    models::ShipmentBusinessSettings result;
    result.revision = revision;
    result.rules = std::move(rules);
    result.planned_containers_allowed = limits.PlannedContainers;
    for (const auto& entry : rows.AsSetOf<models::pg::ShipmentDictionaryValue>())
        if (helpers::ParseDictionary(entry.Dictionary) == helpers::ShipmentDictionary::kRouteType &&
            std::find(limits.RouteTypes.begin(), limits.RouteTypes.end(), entry.Code) !=
                limits.RouteTypes.end())
            result.allowed_route_types.push_back({.code = entry.Code, .name = entry.Name});
    return result;
}

std::vector<std::string> SettingsComponent::GetContainerTypes(bool include_inactive) const {
    const auto catalog = configs_.GetSnapshot().Get(configs::CONTAINER_CATALOG);
    std::vector<std::string> types;
    for (auto group = catalog.begin(); group != catalog.end(); ++group)
        if (include_inactive || (*group)["active"].As<bool>())
            types.push_back(group.GetName());
    std::ranges::sort(types);
    return types;
}

models::ShipmentDictionaries SettingsComponent::GetDictionaries() const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
    models::ShipmentDictionaries result;
    for (const auto& entry : rows.AsSetOf<models::pg::ShipmentDictionaryValue>()) {
        const models::DictionaryValue value{.code = entry.Code, .name = entry.Name};
        switch (helpers::ParseDictionary(entry.Dictionary)) {
        case helpers::ShipmentDictionary::kIncoterms:
            result.incoterms.push_back(value);
            break;
        case helpers::ShipmentDictionary::kCarriageTerms:
            result.carriage_terms.push_back(value);
            break;
        case helpers::ShipmentDictionary::kLoading:
            result.loading.push_back(value);
            break;
        case helpers::ShipmentDictionary::kRouteType:
            result.route_types.push_back(value);
            break;
        }
    }
    return result;
}

models::ShipmentBusinessSettings SettingsComponent::GetBusinessSettings(
    const boost::uuids::uuid& organization) const {
    const auto limits = ReadPlatformSettings(configs_.GetSnapshot(), organization);
    const auto stored = LoadRules(organization);
    const auto effective = ResolveShipmentSettings(limits, limits, stored);
    return BuildBusinessSettings(limits,
        {.number_template = effective.NumberTemplate,
            .planned_containers = effective.PlannedContainers,
            .reset_number_yearly = effective.ResetNumberYearly,
            .route_types = effective.RouteTypes,
            .statuses = effective.Statuses},
        stored.has_value() ? stored.value().Revision : 0);
}

models::ShipmentBusinessSettings SettingsComponent::SaveBusinessSettings(
    const boost::uuids::uuid& organization, const platform::auth::Identity& actor,
    const models::ShipmentBusinessRules& requested_rules, std::int64_t expected_revision) const {
    const auto rules = converters::Convert(requested_rules);
    const auto limits = ReadPlatformSettings(configs_.GetSnapshot(), organization);
    ValidateBusinessRules(limits, rules);
    auto result = BuildBusinessSettings(limits, requested_rules, 0);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto changed = transaction.Execute(
        sql::kSaveOrganizationShipmentRules, organization, expected_revision, rules);
    if (changed.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки уже изменились. Обновите страницу"};
    const auto revision = changed.AsSingleRow<std::int64_t>();
    helpers::WriteAudit(transaction, organization, actor, "update_shipment_rules", organization,
        requested_rules, requested_rules);
    transaction.Commit();
    result.revision = revision;
    return result;
}
}  // namespace shipments
