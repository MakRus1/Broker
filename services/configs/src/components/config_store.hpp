#pragma once

#include <models/config.hpp>
#include <models/draft.hpp>
#include <models/errors.hpp>
#include <models/event.hpp>
#include <models/revision.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace configs {
class ConfigStore final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "config-store";
    ConfigStore(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    bool AllowSelfApproval() const {
        return allow_self_approval_;
    }

    Revision Current() const;
    ConfigValues Values(const std::optional<std::vector<std::string>>& ids) const;
    std::vector<CatalogEntry> Catalog() const;
    Config GetConfig(const std::string& name) const;
    std::vector<RevisionInfo> History() const;
    Draft CreateDraft(const models::CreateConfigDraftRequest& request,
        const platform::auth::Identity& identity) const;
    Draft EditDraft(const models::EditConfigDraftRequest& request,
        const platform::auth::Identity& identity) const;
    Draft ApproveDraft(const models::ConfigDraftActionRequest& request,
        const platform::auth::Identity& identity) const;
    Draft CancelDraft(const models::ConfigDraftActionRequest& request,
        const platform::auth::Identity& identity) const;
    Draft GetDraft(std::int64_t id) const;
    std::vector<Draft> ListDrafts(const models::ListConfigDraftsRequest& filter) const;
    std::vector<DraftEvent> DraftEvents(std::int64_t id) const;

  private:
    void ValidateConfig(const std::string& name, const Json& value) const;
    Revision Current(userver::storages::postgres::Transaction& transaction) const;
    Revision PublishLocked(userver::storages::postgres::Transaction&, const Revision& current,
        const Json& values) const;
    Draft LoadActiveDraft(userver::storages::postgres::Transaction&, std::int64_t id,
        std::int64_t expected_version) const;
    void SaveDraft(userver::storages::postgres::Transaction&, const Draft& draft,
        const std::string& actor, std::string_view action) const;
    void WriteDraftEvent(userver::storages::postgres::Transaction&, const Draft& draft,
        const std::string& actor, std::string_view action) const;
    userver::storages::postgres::ClusterPtr cluster_;
    bool allow_self_approval_;
};
}  // namespace configs

template <> inline constexpr bool userver::components::kHasValidate<configs::ConfigStore> = true;
