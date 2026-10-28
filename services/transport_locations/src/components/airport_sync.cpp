#include <components/airport_sync.hpp>
#include <configs/keys.hpp>
#include <helpers/airports.hpp>
#include <helpers/sync_schedule.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transport_locations {
namespace pg = userver::storages::postgres;

AirportSync::AirportSync(const userver::components::ComponentConfig& config,
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

AirportSync::~AirportSync() {
    task_.Stop();
}

void AirportSync::Sync() {
    const auto policy =
        helpers::MakeSyncPolicy(configs_.GetSnapshot().Get(configs::AIRPORT_SYNC_SETTINGS));
    helpers::RunDirectorySync(
        pg_, std::string{"ourairports"}, policy, 1, [&](const boost::uuids::uuid& lease) {
            // Ни скачивание, ни CSV-парсер не удерживают соединение или транзакцию БД.
            const auto response = http_.CreateRequest()
                                      .get(url_)
                                      .timeout(policy.HttpTimeout)
                                      .retry(policy.HttpRetries + 1)
                                      .perform();
            if (!response->IsOk())
                throw std::runtime_error("OurAirports: HTTP " +
                    std::to_string(static_cast<int>(response->status_code())));
            const auto airports = helpers::ParseAirports(response->body_view(), minimum_);
            const auto hash = userver::crypto::hash::Sha256(response->body_view());
            std::vector<std::int64_t> ids;
            ids.reserve(airports.size());
            for (const auto& airport : airports)
                ids.push_back(airport.SourceId);
            const pg::CommandControl control{std::chrono::seconds{30}, std::chrono::seconds{25}};
            auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{}, control);
            const auto state =
                tx.Execute(sql::kLockDirectorySync, std::string{"ourairports"}, lease);
            if (state.IsEmpty())
                return;
            const auto previous = state.AsSingleRow<int>();
            if (airports.size() * 100 < static_cast<std::size_t>(previous) * 80)
                throw std::runtime_error(
                    "OurAirports: число аэропортов уменьшилось более чем на 20%");
            tx.Execute(sql::kSaveAirportSources, airports);
            tx.Execute(sql::kPublishAirports, ids);
            tx.Execute(sql::kFinishDirectorySync, std::string{"ourairports"}, lease, hash,
                static_cast<int>(airports.size()), std::optional<std::string>{},
                policy.PeriodSeconds);
            tx.Commit();
            LOG_INFO() << "OurAirports: обновлено аэропортов " << airports.size()
                       << ", sha256=" << hash;
        });
}

userver::yaml_config::Schema AirportSync::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Ежедневное обновление общего справочника аэропортов из OurAirports
additionalProperties: false
properties:
    url:
        type: string
        description: Адрес полной CSV-выгрузки OurAirports
    minimum-records:
        type: integer
        minimum: 1
        description: Минимальное допустимое число аэропортов
)");
}
}  // namespace transport_locations
