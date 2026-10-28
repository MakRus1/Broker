#pragma once
#include <algorithm>
#include <chrono>
#include <transport_locations/sql_queries.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/boost_uuid4.hpp>

namespace transport_locations::helpers {
struct SyncPolicy {
    bool Enabled;
    int PeriodSeconds;
    std::chrono::milliseconds HttpTimeout;
    int HttpRetries;
    int RetryInitialSeconds;
    int RetryMaxSeconds;
};

template <typename Settings> SyncPolicy MakeSyncPolicy(const Settings& value) {
    return {value.Enabled, static_cast<int>(value.PeriodSeconds),
        std::chrono::milliseconds{value.HttpTimeoutMs}, static_cast<int>(value.HttpRetries),
        static_cast<int>(value.RetryInitialSeconds), static_cast<int>(value.RetryMaxSeconds)};
}

// Аренда перекрывает HTTP-запросы с повторами и запас для публикации в БД.
// Повторные синки планируются через БД; спящий воркер не держит соединение.
template <typename Callback>
void RunDirectorySync(const userver::storages::postgres::ClusterPtr& cluster,
    const std::string& source, const SyncPolicy& policy, int requests, Callback callback) {
    namespace pg = userver::storages::postgres;
    if (!policy.Enabled)
        return;
    const auto lease = userver::utils::generators::GenerateBoostUuid();
    const auto lease_time = std::max(std::chrono::milliseconds{300000},
        policy.HttpTimeout * (policy.HttpRetries + 1) * requests + std::chrono::minutes{1});
    if (cluster
            ->Execute(pg::ClusterHostType::kMaster, sql::kClaimDirectorySync, source, lease,
                policy.PeriodSeconds, policy.RetryInitialSeconds, policy.RetryMaxSeconds,
                static_cast<int>(lease_time.count()))
            .IsEmpty())
        return;
    try {
        callback(lease);
    } catch (...) {
        try {
            cluster->Execute(pg::ClusterHostType::kMaster, sql::kFailDirectorySync, source, lease,
                policy.RetryInitialSeconds, policy.RetryMaxSeconds);
        } catch (const std::exception& error) {
            LOG_WARNING() << "Не удалось сохранить расписание повтора " << source << ": "
                          << error.what();
        }
        throw;
    }
}
}  // namespace transport_locations::helpers
