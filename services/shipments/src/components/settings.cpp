#include <algorithm>
#include <components/settings.hpp>
#include <converters/business_rules.hpp>
#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <models/pg/organization_shipment_rules.hpp>
#include <models/pg/shipment_business_rules.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;
using namespace converters;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      configs_(context.FindComponent<userver::components::DynamicConfig>().GetSource()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      card_integrations_(context.FindComponent<ShipmentCardIntegrations>()) {}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

namespace {
configs::ShipmentSettings ReadPlatformSettings(const platform::configs::Snapshot& snapshot,
    const std::string& organization, const std::optional<std::string>& customer = std::nullopt) {
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

std::optional<models::pg::OrganizationShipmentRules> ReadStoredRules(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<models::pg::OrganizationShipmentRules>();
}

configs::ShipmentSettings ResolveEffectiveSettings(const platform::configs::Snapshot& snapshot,
    const std::string& organization, const std::optional<std::string>& customer,
    const std::optional<models::pg::OrganizationShipmentRules>& stored) {
    const auto limits = ReadPlatformSettings(snapshot, organization);
    auto defaults =
        customer.has_value() ? ReadPlatformSettings(snapshot, organization, customer) : limits;
    return ResolveShipmentSettings(std::move(defaults), limits, stored);
}

}  // namespace

std::optional<models::pg::OrganizationShipmentRules> SettingsComponent::LoadRules(
    const std::string& organization) const {
    return ReadStoredRules(cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOrganizationShipmentRules, organization));
}

configs::ShipmentSettings SettingsComponent::Effective(
    const std::string& organization, const std::optional<std::string>& customer) const {
    return ResolveEffectiveSettings(
        configs_.GetSnapshot(), organization, customer, LoadRules(organization));
}

configs::ShipmentSettings SettingsComponent::Effective(pg::Transaction& transaction,
    const std::string& organization, const std::optional<std::string>& customer) const {
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
    result.allowed_container_types = limits.ContainerTypes;
    result.planned_containers_allowed = limits.PlannedContainers;
    for (const auto& entry : rows.AsSetOf<models::pg::ShipmentDictionaryValue>())
        if (ParseDictionary(entry.Dictionary) == ShipmentDictionary::kRouteType &&
            std::find(limits.RouteTypes.begin(), limits.RouteTypes.end(), entry.Code) !=
                limits.RouteTypes.end())
            result.allowed_route_types.push_back({.code = entry.Code, .name = entry.Name});
    return result;
}

models::ShipmentSettings SettingsComponent::Get(const models::GetShipmentSettingsRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto settings = Effective(organization, request.customer_id);
    const auto customer_logist = request.customer_id.has_value()
        ? card_integrations_.CustomerLogist(context, request.customer_id.value())
        : std::nullopt;
    return models::ShipmentSettings{.container_types = settings.ContainerTypes,
        .customer_logist = customer_logist,
        .number_template = settings.NumberTemplate,
        .planned_containers = settings.PlannedContainers,
        .reset_number_yearly = settings.ResetNumberYearly,
        .route_types = settings.RouteTypes,
        .statuses = settings.Statuses};
}

models::ShipmentDictionaries SettingsComponent::Dictionaries(
    const models::SettingsDictionariesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
    models::ShipmentDictionaries result;
    for (const auto& entry : rows.AsSetOf<models::pg::ShipmentDictionaryValue>()) {
        const models::DictionaryValue value{.code = entry.Code, .name = entry.Name};
        switch (ParseDictionary(entry.Dictionary)) {
        case ShipmentDictionary::kIncoterms:
            result.incoterms.push_back(value);
            break;
        case ShipmentDictionary::kCarriageTerms:
            result.carriage_terms.push_back(value);
            break;
        case ShipmentDictionary::kLoading:
            result.loading.push_back(value);
            break;
        case ShipmentDictionary::kRouteType:
            result.route_types.push_back(value);
            break;
        }
    }
    return result;
}

models::ShipmentBusinessSettings SettingsComponent::GetBusiness(
    const models::SettingsGetBusinessRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        platform::auth::RequireOrganizationAdministrator(
            organizations_, context, request.organization_id);

        const auto limits = ReadPlatformSettings(configs_.GetSnapshot(), request.organization_id);
        const auto stored = LoadRules(request.organization_id);
        const auto effective = ResolveShipmentSettings(limits, limits, stored);
        return BuildBusinessSettings(limits,
            {.container_types = effective.ContainerTypes,
                .number_template = effective.NumberTemplate,
                .planned_containers = effective.PlannedContainers,
                .reset_number_yearly = effective.ResetNumberYearly,
                .route_types = effective.RouteTypes,
                .statuses = effective.Statuses},
            stored.has_value() ? stored.value().Revision : 0);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"not_found", "Организация не найдена"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ проверки доступа"};
    }
}

models::ShipmentBusinessSettings SettingsComponent::UpdateBusiness(
    const models::SettingsUpdateBusinessRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        platform::auth::RequireOrganizationAdministrator(
            organizations_, context, request.organization_id);

        const auto rules = Convert(request.rules);
        const auto limits = ReadPlatformSettings(configs_.GetSnapshot(), request.organization_id);
        ValidateBusinessRules(limits, rules);
        auto result = BuildBusinessSettings(limits, request.rules, 0);
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto changed = transaction.Execute(sql::kSaveOrganizationShipmentRules,
            request.organization_id, request.expected_revision, rules);
        if (changed.IsEmpty())
            throw ApiError<409>{"revision_conflict", "Настройки уже изменились. Обновите страницу"};
        const auto revision = changed.AsSingleRow<std::int64_t>();
        WriteAudit(transaction, request.organization_id, platform::auth::GetIdentity(context),
            "update_shipment_rules", request.organization_id, request.rules, request.rules);
        transaction.Commit();
        result.revision = revision;
        return result;
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"not_found", "Организация не найдена"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ проверки доступа"};
    }
}
}  // namespace shipments
