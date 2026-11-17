#include <components/railway_stations.hpp>
#include <converters/locations.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <models/pg/railway_station_raw.hpp>
#include <platform/registry_read.hpp>
#include <registry/stations.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/utils/text.hpp>

namespace transport_locations {
using namespace helpers;
using namespace converters;
using namespace registry_query;

RailwayStationsComponent::RailwayStationsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transport_locations")
                   .GetCluster()) {}

models::StationListResult RailwayStationsComponent::List(
    const models::ListStationsRequest& request) const {
    const auto scope = registry::CursorScope(request, "railway_stations");
    if (!registry::CursorMatches(request, scope, false))
        throw ApiError<400>{"invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
    const auto query = StationQuery(request);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    auto rows =
        platform::registry::ExecuteWithCustomPlan(transaction, query)
            .AsContainer<std::vector<registry::Row<models::pg::RailwayStationRaw>>>(pg::kRowTag);
    const bool more = rows.size() > static_cast<std::size_t>(request.limit);
    if (more)
        rows.pop_back();
    models::StationListResult result;
    result.items = registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
    if (more) {
        result.next = userver::utils::ToString(rows.back().Value.Id);
        result.next_cursor = models::StationListCursor{
            .id = rows.back().Value.Id, .scope = scope, .value = rows.back().SortKey};
    }
    transaction.Commit();
    return result;
}

models::RailwayStation RailwayStationsComponent::Save(
    const models::SaveStationRequest& request) const {
    const auto name = userver::utils::text::Trim(request.name);
    if (name.empty())
        throw ApiError<400>{"invalid_name", "Укажите название станции"};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kSaveRailwayStation,
        request.id, name, request.country_code, request.esr_code, request.uic_code,
        request.existing.value_or(false));
    if (rows.IsEmpty())
        throw ApiError<409>{"station_exists", "Идентификатор станции уже используется"};
    return Convert(rows.AsSingleRow<models::pg::RailwayStationRaw>());
}

void RailwayStationsComponent::Archive(const boost::uuids::uuid& id) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kArchiveRailwayStation, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"station_not_found", "Станция не найдена"};
}

std::vector<models::RailwayStation> RailwayStationsComponent::GetBatch(
    const std::vector<boost::uuids::uuid>& requested_ids) const {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetRailwayStations, requested_ids);
    std::vector<models::RailwayStation> result;
    for (const auto& row : rows.AsSetOf<models::pg::RailwayStationRaw>())
        result.push_back(Convert(row));
    return result;
}

std::vector<models::RailwayStation> RailwayStationsComponent::Search(
    const std::string& search) const {
    const auto query = userver::utils::text::Trim(search);
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kSlave, sql::kSearchRailwayStations, query);
    std::vector<models::RailwayStation> result;
    for (const auto& row : rows.AsSetOf<models::pg::RailwayStationRaw>())
        result.push_back(Convert(row));
    return result;
}
}  // namespace transport_locations
