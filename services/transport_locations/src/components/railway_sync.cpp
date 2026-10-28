#include <components/railway_sync.hpp>
#include <configs/keys.hpp>
#include <helpers/railway_query.hpp>
#include <helpers/railway_source.hpp>
#include <helpers/sync_schedule.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/dynamic_config/storage/component.hpp>
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
      configs_(context.FindComponent<userver::components::DynamicConfig>().GetSource()),
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
    const auto policy =
        helpers::MakeSyncPolicy(configs_.GetSnapshot().Get(configs::RAILWAY_SYNC_SETTINGS));
    helpers::RunDirectorySync(
        pg_, std::string{"osm-railway"}, policy, 1, [&](const boost::uuids::uuid& lease) {
            // Ни скачивание, ни JSON-парсер не удерживают соединение или транзакцию БД.
            const auto response =
                http_.CreateRequest()
                    .get(url_ + "?" +
                        userver::http::MakeQuery({{"query", std::string{helpers::kRailwayQuery}}}))
                    .headers({{"Accept", "application/sparql-results+json"},
                        {"User-Agent", "Broker-transport-locations/1.0"}})
                    .timeout(policy.HttpTimeout)
                    .retry(policy.HttpRetries + 1)
                    .perform();
            if (!response->IsOk())
                throw std::runtime_error("OSM railway: HTTP " +
                    std::to_string(static_cast<int>(response->status_code())));
            if (response->body_view().size() > 64 * 1024 * 1024)
                throw std::runtime_error("OSM railway: превышен размер ответа");
            const auto stations = helpers::ParseRailwaySource(
                userver::formats::json::FromString(response->body()), minimum_);
            const auto hash = userver::crypto::hash::Sha256(response->body_view());
            std::vector<std::string> ids;
            ids.reserve(stations.size());
            for (const auto& station : stations)
                ids.push_back(station.SourceId);
            auto tx = pg_->Begin(
                "directory-sync.railway", pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            const auto state =
                tx.Execute(sql::kLockDirectorySync, std::string{"osm-railway"}, lease);
            if (state.IsEmpty())
                return;
            const auto previous = state.AsSingleRow<int>();
            if (stations.size() * 100 < static_cast<std::size_t>(previous) * 80)
                throw std::runtime_error("OSM railway: число станций уменьшилось более чем на 20%");
            tx.Execute(sql::kSaveRailwaySources, stations);
            tx.Execute(sql::kPublishRailwayStations, ids);
            tx.Execute(sql::kFinishDirectorySync, std::string{"osm-railway"}, lease, hash,
                static_cast<int>(stations.size()), std::optional<std::string>{},
                policy.PeriodSeconds);
            tx.Commit();
            LOG_INFO() << "OSM railway: обновлено станций " << stations.size()
                       << ", sha256=" << hash;
        });
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
    minimum-records:
        type: integer
        minimum: 1
        description: Минимальное допустимое число станций
)");
}
}  // namespace transport_locations
