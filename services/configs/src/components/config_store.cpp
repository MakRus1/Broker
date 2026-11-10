#include <components/config_store.hpp>

#include <components/draft_policy.hpp>
#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace pg = userver::storages::postgres;

}  // namespace

namespace configs {
ConfigStore::ConfigStore(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-configs").GetCluster()),
      allow_self_approval_(context.FindComponent<DraftPolicy>().AllowSelfApproval()) {}

userver::yaml_config::Schema ConfigStore::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

Revision ConfigStore::Current() const {
    return cluster_->Execute(pg::ClusterHostType::kMaster, sql::kLastRevision)
        .AsSingleRow<Revision>(pg::kRowTag);
}

Revision ConfigStore::Current(pg::Transaction& transaction) const {
    return transaction.Execute(sql::kLastRevision).AsSingleRow<Revision>(pg::kRowTag);
}

ConfigValues ConfigStore::Values(const std::optional<std::vector<std::string>>& ids) const {
    const auto current = Current();
    userver::formats::json::ValueBuilder values{userver::formats::json::Type::kObject};
    if (ids.has_value()) {
        for (const auto& id : ids.value())
            if (current.Values.HasMember(id))
                values[id] = current.Values[id];
    } else {
        values = userver::formats::json::ValueBuilder{current.Values};
    }
    return {values.ExtractValue(), current.Id, current.PublishedAt};
}

Revision ConfigStore::PublishLocked(pg::Transaction& transaction, const Revision& current,
    const platform::configs::Json& values) const {
    // Откат и подтверждение тоже проверяются по текущим схемам.
    for (auto it = values.begin(); it != values.end(); ++it)
        ValidateConfig(it.GetName(), *it);
    const auto published =
        transaction.Execute(sql::kInsertRevision, values).AsSingleRow<Revision>(pg::kRowTag);
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!current.Values.HasMember(it.GetName()) || current.Values[it.GetName()] != *it)
            transaction.Execute(sql::kSetConfigVersion, it.GetName(), published.Id);
    }
    for (auto it = current.Values.begin(); it != current.Values.end(); ++it) {
        if (!values.HasMember(it.GetName()))
            transaction.Execute(sql::kSetConfigVersion, it.GetName(), published.Id);
    }
    return published;
}

std::vector<RevisionInfo> ConfigStore::History() const {
    return cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistory)
        .AsContainer<std::vector<RevisionInfo>>(pg::kRowTag);
}

}  // namespace configs
