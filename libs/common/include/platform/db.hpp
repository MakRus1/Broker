#pragma once
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

#include <variant>

namespace platform::db {
namespace pg = userver::storages::postgres;

// Согласованное чтение нескольких запросов без записи.
inline constexpr pg::TransactionOptions kReadOnlySnapshot{
    pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly};

// Где выполнить запрос: в открытой транзакции сценария или отдельным запросом к кластеру.
// Функции чтения принимают Executor и не нужны в двух вариантах — с транзакцией и без.
// Функции записи и блокировки принимают pg::Transaction&.
class Executor {
  public:
    Executor(pg::Transaction& transaction) : target_(&transaction) {}

    Executor(const pg::ClusterPtr& cluster, pg::ClusterHostType host = pg::ClusterHostType::kMaster)
        : target_(ClusterTarget{cluster.get(), host}) {}

    template <typename... Args>
    pg::ResultSet Execute(const pg::Query& query, const Args&... args) const {
        if (const auto* transaction = std::get_if<pg::Transaction*>(&target_))
            return (*transaction)->Execute(query, args...);
        const auto& cluster = std::get<ClusterTarget>(target_);
        return cluster.Cluster->Execute(cluster.Host, query, args...);
    }

  private:
    struct ClusterTarget {
        pg::Cluster* Cluster;
        pg::ClusterHostType Host;
    };

    std::variant<pg::Transaction*, ClusterTarget> target_;
};
}  // namespace platform::db
