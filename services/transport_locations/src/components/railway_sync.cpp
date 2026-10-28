#include <components/railway_sync.hpp>
#include <helpers/railway_query.hpp>
#include <helpers/railway_source.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transport_locations {
namespace pg = userver::storages::postgres;

RailwaySync::RailwaySync(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-transport_locations")
              .GetCluster()),
      url_(config["url"].As<std::string>()),
      timeout_(config["timeout"].As<std::chrono::milliseconds>()),
      minimum_(config["minimum-records"].As<std::size_t>()) {
    userver::utils::PeriodicTask::Settings settings{
        std::chrono::minutes{1}, userver::utils::PeriodicTask::Flags::kNow};
    settings.exception_period = std::chrono::minutes{1};
    userver::utils::StartPeriodicTask(
        task_, std::string{kName}, settings, [this] { Sync(); },
        context.FindComponent<userver::components::TestsuiteSupport>().GetTestsuiteTasks());
}

RailwaySync::~RailwaySync() {
    task_.Stop();
}

void RailwaySync::Sync() {
    const auto lease = userver::utils::generators::GenerateBoostUuid();
    if (pg_->Execute(pg::ClusterHostType::kMaster, sql::kClaimDirectorySync,
               std::string{"osm-railway"}, lease)
            .IsEmpty())
        return;
    // Ни скачивание, ни JSON-парсер не удерживают соединение или транзакцию БД.
    const auto response =
        http_.CreateRequest()
            .get(url_ + "?" +
                userver::http::MakeQuery({{"query", std::string{helpers::kRailwayQuery}}}))
            .headers({{"Accept", "application/sparql-results+json"},
                {"User-Agent", "Broker-transport-locations/1.0"}})
            .timeout(timeout_)
            .perform();
    if (!response->IsOk())
        throw std::runtime_error(
            "OSM railway: HTTP " + std::to_string(static_cast<int>(response->status_code())));
    if (response->body_view().size() > 64 * 1024 * 1024)
        throw std::runtime_error("OSM railway: превышен размер ответа");
    const auto stations =
        helpers::ParseRailwaySource(userver::formats::json::FromString(response->body()), minimum_);
    const auto hash = userver::crypto::hash::Sha256(response->body_view());
    std::vector<std::string> ids;
    ids.reserve(stations.size());
    for (const auto& station : stations)
        ids.push_back(station.SourceId);
    const pg::CommandControl control{std::chrono::seconds{30}, std::chrono::seconds{25}};
    auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{}, control);
    const auto state = tx.Execute(sql::kLockDirectorySync, std::string{"osm-railway"}, lease);
    if (state.IsEmpty())
        return;
    const auto previous = state.AsSingleRow<int>();
    if (stations.size() * 100 < static_cast<std::size_t>(previous) * 80)
        throw std::runtime_error("OSM railway: число станций уменьшилось более чем на 20%");
    tx.Execute(sql::kSaveRailwaySources, stations);
    tx.Execute(sql::kPublishRailwayStations, ids);
    tx.Execute(sql::kFinishRailwaySync, std::string{"osm-railway"}, lease, hash,
        static_cast<int>(stations.size()), std::optional<std::string>{});
    tx.Commit();
    LOG_INFO() << "OSM railway: обновлено станций " << stations.size() << ", sha256=" << hash;
}

userver::yaml_config::Schema RailwaySync::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Еженедельное обновление общего справочника станций из OSM
additionalProperties: false
properties:
    url:
        type: string
        description: SPARQL endpoint QLever с полной выгрузкой OSM
    timeout:
        type: string
        description: Максимальное время скачивания
    minimum-records:
        type: integer
        minimum: 1
        description: Минимальное допустимое число станций
)");
}
}  // namespace transport_locations
