#include <components/sea_port_sync.hpp>
#include <helpers/sea_ports.hpp>
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

SeaPortSync::SeaPortSync(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-transport_locations")
              .GetCluster()),
      releases_url_(config["releases-url"].As<std::string>()),
      download_base_url_(config["download-base-url"].As<std::string>()),
      timeout_(config["timeout"].As<std::chrono::milliseconds>()),
      minimum_(config["minimum-records"].As<std::size_t>()) {
    userver::utils::PeriodicTask::Settings settings{
        std::chrono::minutes{1}, userver::utils::PeriodicTask::Flags::kNow};
    settings.exception_period = std::chrono::minutes{1};
    userver::utils::StartPeriodicTask(
        task_, std::string{kName}, settings, [this] { Sync(); },
        context.FindComponent<userver::components::TestsuiteSupport>().GetTestsuiteTasks());
}

SeaPortSync::~SeaPortSync() {
    task_.Stop();
}

void SeaPortSync::Sync() {
    const std::string source = "unlocode";
    const auto lease = userver::utils::generators::GenerateBoostUuid();
    if (pg_->Execute(pg::ClusterHostType::kMaster, sql::kClaimDirectorySync, source, lease)
            .IsEmpty())
        return;
    const auto download = [this](const std::string& url) {
        const auto response = http_.CreateRequest().get(url).timeout(timeout_).perform();
        if (!response->IsOk())
            throw std::runtime_error(
                "UN/LOCODE: HTTP " + std::to_string(static_cast<int>(response->status_code())));
        if (response->body_view().size() > 16 * 1024 * 1024)
            throw std::runtime_error("UN/LOCODE: превышен размер ответа");
        return response->body();
    };
    const auto release = helpers::LatestLocodeRelease(download(releases_url_));
    std::array<std::string, 3> parts;
    for (std::size_t part = 0; part < parts.size(); ++part) {
        parts[part] =
            download(download_base_url_ + release + "/raw/release/csv/UNLOCODE%20CodeListPart" +
                std::to_string(part + 1) + ".csv?job=package-release");
    }
    const auto ports = helpers::ParseSeaPorts(parts, minimum_, std::stoi(release.substr(0, 4)));
    const auto hash = userver::crypto::hash::Sha256({parts[0], parts[1], parts[2]});
    std::vector<std::string> codes;
    codes.reserve(ports.size());
    for (const auto& port : ports)
        codes.push_back(port.Locode);
    auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto state = tx.Execute(sql::kLockDirectorySync, source, lease);
    if (state.IsEmpty())
        return;
    const auto previous = state.AsSingleRow<int>();
    if (ports.size() * 100 < static_cast<std::size_t>(previous) * 80)
        throw std::runtime_error("UN/LOCODE: число портов уменьшилось более чем на 20%");
    const pg::CommandControl control{std::chrono::seconds{30}, std::chrono::seconds{25}};
    tx.Execute(control, sql::kSaveSeaPortSources, ports);
    tx.Execute(control, sql::kPublishSeaPorts, codes);
    tx.Execute(
        sql::kFinishDirectorySync, source, lease, hash, static_cast<int>(ports.size()), release);
    tx.Commit();
    LOG_INFO() << "UN/LOCODE: выпуск " << release << ", обновлено портов " << ports.size()
               << ", sha256=" << hash;
}

userver::yaml_config::Schema SeaPortSync::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Обновление морских портов из официального выпуска UN/LOCODE
additionalProperties: false
properties:
    releases-url:
        type: string
        description: Список официальных тегов выпуска
    download-base-url:
        type: string
        description: Базовый адрес CSV-артефактов официального репозитория
    timeout:
        type: string
        description: Максимальное время одного HTTP-запроса
    minimum-records:
        type: integer
        minimum: 1
        description: Минимальное допустимое число портов
)");
}
}  // namespace transport_locations
