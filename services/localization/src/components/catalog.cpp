#include <components/catalog.hpp>
#include <converters/catalog.hpp>
#include <localization/sql_queries.hpp>
#include <platform/handler_error.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/utils/boost_uuid4.hpp>

namespace localization {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

CatalogComponent::CatalogComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

void CatalogComponent::CheckOrganization(
    const std::string& access_token, const boost::uuids::uuid& organization) const {
    namespace api = clients::organizations::ApiOrganizationsMembershipGet;
    try {
        organizations_.ApiOrganizationsMembershipGet(
            api::Request{.body = {.organization_id = organization}}, access_token);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    }
}

std::vector<models::Translation> CatalogComponent::GetOrganizationCatalog(
    const models::GetCatalogRequest& request, const std::string& access_token) const {
    CheckOrganization(access_token, request.organization_id);
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetCatalog,
        request.organization_id, models::EnumValue(request.locale));
    return converters::ConvertMessages(rows.AsSingleRow<json::Value>());
}

std::vector<models::Translation> CatalogComponent::GetPublicCatalog(
    const std::string& locale) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetPublicCatalog, locale);
    std::vector<models::Translation> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<models::PublicTranslationRow>(pg::kRowTag))
        result.push_back(converters::Convert(row));
    return result;
}
}  // namespace localization
