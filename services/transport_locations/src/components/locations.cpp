#include <boost/uuid/string_generator.hpp>
#include <components/locations.hpp>
#include <converters/locations.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <registry/locations.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transport_locations {
using namespace helpers;
using namespace converters;
using namespace registry_query;

LocationsComponent::LocationsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transport_locations")
                   .GetCluster()) {}

userver::yaml_config::Schema LocationsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::LocationListResult LocationsComponent::List(
    const models::ListLocationsRequest& request) const {
    const auto scope = registry::CursorScope(request, "route_locations");
    if (!registry::CursorMatches(request, scope, EnumValue(request.sort) == "id"))
        throw ApiError<400>{"invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
    const auto query = LocationQuery(request);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    transaction.Execute(sql::kRegistryPlan);
    auto rows =
        transaction.Execute(query.Statement, query.Parameters)
            .AsContainer<std::vector<registry::Row<pgtypes::TransportLocationV1>>>(pg::kRowTag);
    const bool more = rows.size() > static_cast<std::size_t>(request.limit);
    if (more)
        rows.pop_back();
    models::LocationListResult result;
    result.items = registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
    if (more) {
        result.next = boost::uuids::to_string(rows.back().Value.Id);
        result.next_cursor =
            models::LocationListCursor{.id = boost::uuids::to_string(rows.back().Value.Id),
                .scope = scope,
                .value = rows.back().SortKey};
    }
    transaction.Commit();
    return result;
}

models::TransportLocation LocationsComponent::Save(
    const models::SaveLocationRequest& request) const {
    try {
        const auto name = userver::utils::text::Trim(request.name);
        if (name.empty())
            throw ApiError<400>{"invalid_name", "Укажите название"};
        auto code = request.code;
        if (code.has_value()) {
            code = userver::utils::text::Trim(code.value());
            if (code.value().empty())
                code.reset();
        }
        const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kSaveRouteLocation,
            request.id, models::EnumValue(request.kind), name, request.country_code, code,
            request.existing.value_or(false));
        if (rows.IsEmpty())
            throw ApiError<409>{"location_exists", "Запись уже существует или имеет другой тип"};
        return Convert(rows.AsSingleRow<pgtypes::TransportLocationV1>());
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"location_code_exists", "Код уже используется в этой стране"};
    }
}

void LocationsComponent::Archive(const std::string& id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kArchiveRouteLocation, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"location_not_found", "Запись не найдена"};
}

std::vector<models::TransportLocation> LocationsComponent::GetBatch(
    const std::vector<std::string>& requested_ids) const {
    std::vector<boost::uuids::uuid> ids;
    ids.reserve(requested_ids.size());
    for (const auto& id : requested_ids)
        ids.push_back(boost::uuids::string_generator{}(id));
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetRouteLocations, ids);
    std::vector<models::TransportLocation> result;
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>())
        result.push_back(Convert(row));
    return result;
}

std::vector<models::TransportLocation> LocationsComponent::Search(
    models::TransportLocationKind kind, const std::optional<std::string>& query) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kSearchRouteLocations,
        models::EnumValue(kind), userver::utils::text::Trim(query.value_or("")));
    std::vector<models::TransportLocation> result;
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>()) {
        result.push_back(Convert(row));
    }
    return result;
}
}  // namespace transport_locations
