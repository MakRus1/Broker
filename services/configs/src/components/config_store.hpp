#pragma once

#include <domain/models.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace configs {
class ConfigStore final {
  public:
    explicit ConfigStore(userver::storages::postgres::ClusterPtr cluster)
        : cluster_(std::move(cluster)) {}
    Revision Current() const;
    ConfigValues Values(const std::optional<std::vector<std::string>>& ids) const;
    std::vector<CatalogEntry> Catalog() const;
    Config GetConfig(const std::string& name) const;
    std::vector<RevisionInfo> History() const;
    Draft CreateDraft(const CreateDraftRequest& request) const;
    Draft EditDraft(const EditDraftRequest& request) const;
    Draft ApproveDraft(const DraftActionRequest& request) const;
    Draft CancelDraft(const DraftActionRequest& request) const;
    Draft GetDraft(std::int64_t id) const;
    std::vector<Draft> ListDrafts(const DraftFilter& filter) const;
    std::vector<DraftEvent> DraftEvents(std::int64_t id) const;

  private:
    Revision Current(userver::storages::postgres::Transaction& transaction) const;
    Revision PublishLocked(userver::storages::postgres::Transaction&, const Revision& current,
                           const Json& values) const;
    Draft LoadActiveDraft(userver::storages::postgres::Transaction&, std::int64_t id,
                          std::int64_t expected_version) const;
    void SaveDraft(userver::storages::postgres::Transaction&, const Draft& draft,
                   const std::string& actor, std::string_view action) const;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace configs
