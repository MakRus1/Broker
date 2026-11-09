#include <components/numbering.hpp>
#include <contracts/sql_queries.hpp>
#include <helpers/numbering.hpp>
#include <limits>
#include <models/errors.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace contracts {
namespace pg = userver::storages::postgres;

NumberingComponent::NumberingComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-contracts").GetCluster()) {
}

userver::yaml_config::Schema NumberingComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void NumberingComponent::RequireAdministrator(
    const userver::server::request::RequestContext& context,
    const std::string& organization) const {
    try {
        platform::auth::RequireOrganizationAdministrator(organizations_, context, organization);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }
}

models::ContractNumbering NumberingComponent::Get(
    const std::string& organization, userver::server::request::RequestContext& context) const {
    RequireAdministrator(context, organization);
    return helpers::ConvertNumbering(
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetNumbering, organization));
}

models::ContractNumbering NumberingComponent::Update(
    const models::UpdateContractNumberingRequest& body,
    userver::server::request::RequestContext& context) const {
    RequireAdministrator(context, body.organization_id);
    platform::ValidateNumberTemplate(body.number_template);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kSaveNumbering, body.organization_id,
            body.number_template, body.reset_number_yearly, body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    return helpers::ConvertNumbering(rows);
}
}  // namespace contracts
