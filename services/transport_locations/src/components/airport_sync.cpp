#include <components/airport_sync.hpp>
#include <helpers/airports.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/hash.hpp>
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
      timeout_(config["timeout"].As<std::chrono::milliseconds>()),
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
    const auto lease = userver::utils::generators::GenerateBoostUuid();
    if (pg_->Execute(pg::ClusterHostType::kMaster, sql::kClaimDirectorySync,
               std::string{"ourairports"}, lease)
            .IsEmpty())
        return;
    // Ни скачивание, ни CSV-парсер не удерживают соединение или транзакцию БД.
    const auto response = http_.CreateRequest().get(url_).timeout(timeout_).perform();
    if (!response->IsOk())
        throw std::runtime_error(
            "OurAirports: HTTP " + std::to_string(static_cast<int>(response->status_code())));
    const auto airports = helpers::ParseAirports(response->body_view(), minimum_);
    const auto hash = userver::crypto::hash::Sha256(response->body_view());
    std::vector<std::int64_t> ids;
    ids.reserve(airports.size());
    for (const auto& airport : airports)
        ids.push_back(airport.SourceId);
    auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto state = tx.Execute(sql::kLockDirectorySync, std::string{"ourairports"}, lease);
    if (state.IsEmpty())
        return;
    const auto previous = state.AsSingleRow<int>();
    if (airports.size() * 100 < static_cast<std::size_t>(previous) * 80)
        throw std::runtime_error("OurAirports: число аэропортов уменьшилось более чем на 20%");
    const pg::CommandControl control{std::chrono::seconds{30}, std::chrono::seconds{25}};
    tx.Execute(control, sql::kSaveAirportSources, airports);
    tx.Execute(control, sql::kPublishAirports, ids);
    tx.Execute(sql::kFinishDirectorySync, std::string{"ourairports"}, lease, hash,
        static_cast<int>(airports.size()), std::optional<std::string>{});
    tx.Commit();
    LOG_INFO() << "OurAirports: обновлено аэропортов " << airports.size() << ", sha256=" << hash;
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
    timeout:
        type: string
        description: Максимальное время скачивания
    minimum-records:
        type: integer
        minimum: 1
        description: Минимальное допустимое число аэропортов
)");
}
}  // namespace transport_locations
