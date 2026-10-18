#pragma once

#include <platform/configs/runtime.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace configs {

struct Revision {
    std::int64_t Id;
    platform::configs::Json Values;
};
class RevisionConflict : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class DraftForbidden : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionNotFound : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class ConfigStore final {
  public:
    explicit ConfigStore(userver::storages::postgres::ClusterPtr cluster)
        : cluster_(std::move(cluster)) {}
    Revision Current() const;
    platform::configs::Json Values(const std::optional<std::vector<std::string>>& ids) const;
    platform::configs::Json Catalog() const;
    platform::configs::Json GetConfig(const std::string& name) const;
    platform::configs::Json History() const;
    platform::configs::Json Draft(std::string_view action,
                                  const platform::configs::Json& request) const;
    platform::configs::Json DraftEvents(std::int64_t id) const;

  private:
    Revision PublishLocked(userver::storages::postgres::Transaction&, const Revision& current,
                           const platform::configs::Json& values) const;

    userver::storages::postgres::ClusterPtr cluster_;
};

}  // namespace configs
