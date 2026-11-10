#include <algorithm>
#include <components/directory_sync.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transport_locations {
DirectorySyncComponent::DirectorySyncComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transport_locations")
                   .GetCluster()) {}

userver::yaml_config::Schema DirectorySyncComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

// Аренда перекрывает HTTP-запросы с повторами и запас для публикации в БД.
// Повторы планируются через БД; ожидание не держит соединение.
void DirectorySyncComponent::Run(const std::string& source, const SyncPolicy& policy, int requests,
    const std::function<void(const boost::uuids::uuid&)>& sync) const {
    namespace pg = userver::storages::postgres;
    if (!policy.Enabled)
        return;
    const auto lease = userver::utils::generators::GenerateBoostUuid();
    const auto lease_time = std::max(std::chrono::milliseconds{300000},
        policy.HttpTimeout * (policy.HttpRetries + 1) * requests + std::chrono::minutes{1});
    if (cluster_
            ->Execute(pg::ClusterHostType::kMaster, sql::kClaimDirectorySync, source, lease,
                policy.PeriodSeconds, policy.RetryInitialSeconds, policy.RetryMaxSeconds,
                static_cast<int>(lease_time.count()))
            .IsEmpty())
        return;
    try {
        sync(lease);
    } catch (...) {
        try {
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFailDirectorySync, source, lease,
                policy.RetryInitialSeconds, policy.RetryMaxSeconds);
        } catch (const std::exception& error) {
            LOG_WARNING() << "Не удалось сохранить расписание повтора " << source << ": "
                          << error.what();
        }
        throw;
    }
}
}  // namespace transport_locations
