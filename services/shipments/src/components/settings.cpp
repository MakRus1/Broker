#include <algorithm>
#include <components/settings.hpp>
#include <converters/business_rules.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <postgresql/types.hpp>
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
      references_(context.FindComponent<ReferencesComponent>()) {}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

configs::detail_SHIPMENT_SETTINGS::Value SettingsComponent::Platform(
    const std::string& organization, const std::optional<std::string>& customer) const {
    return configs_.GetSnapshot().Get(configs::SHIPMENT_SETTINGS,
        config_contexts::ShipmentOrganizationContext{
            .CustomerId = customer, .OrganizationId = organization});
}

std::optional<pgtypes::OrganizationShipmentRulesV1> SettingsComponent::LoadRules(
    const std::string& organization) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOrganizationShipmentRules, organization);
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<pgtypes::OrganizationShipmentRulesV1>();
}

configs::detail_SHIPMENT_SETTINGS::Value SettingsComponent::Effective(
    const std::string& organization, const std::optional<std::string>& customer) const {
    auto result = Platform(organization, customer);
    const auto limits = Platform(organization);
    const auto stored = LoadRules(organization);
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

void SettingsComponent::ValidateBusinessRules(
    const std::string& organization, const pgtypes::ShipmentBusinessRulesV1& rules) const {
    platform::ValidateNumberTemplate(rules.NumberTemplate);
    const auto limits = Platform(organization);
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

models::ShipmentBusinessSettings SettingsComponent::Business(
    const std::string& organization) const {
    const auto limits = Platform(organization);
    const auto effective = Effective(organization);
    const auto stored = LoadRules(organization);
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
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

models::ShipmentSettings SettingsComponent::Get(const models::GetShipmentSettingsRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto settings = Effective(organization, request.customer_id);
    const auto customer_logist = request.customer_id.has_value()
        ? references_.CustomerLogist(context, request.customer_id.value())
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
    for (const auto& entry : rows.AsSetOf<pgtypes::ShipmentDictionaryValueV1>()) {
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

        const auto result = Business(request.organization_id);
        return models::ShipmentBusinessSettings{
            .allowed_container_types = result.allowed_container_types,
            .allowed_route_types = result.allowed_route_types,
            .planned_containers_allowed = result.planned_containers_allowed,
            .revision = result.revision,
            .rules = result.rules};
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
        ValidateBusinessRules(request.organization_id, rules);
        auto result = Business(request.organization_id);
        result.rules = request.rules;
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
        return models::ShipmentBusinessSettings{
            .allowed_container_types = result.allowed_container_types,
            .allowed_route_types = result.allowed_route_types,
            .planned_containers_allowed = result.planned_containers_allowed,
            .revision = result.revision,
            .rules = result.rules};
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
