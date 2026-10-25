#include <components/config_store.hpp>

#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace {
namespace pg = userver::storages::postgres;

configs::Revision WithDefaults(const pg::ResultSet& rows) {
    auto defaults = configs::Defaults();
    auto revision = rows.AsSingleRow<configs::Revision>(pg::kRowTag);
    userver::formats::json::ValueBuilder merged{defaults};
    for (auto it = revision.Values.begin(); it != revision.Values.end(); ++it)
        merged[it.GetName()] = *it;
    revision.Values = merged.ExtractValue();
    return revision;
}
}  // namespace

namespace configs {

Revision ConfigStore::Current() const {
    return WithDefaults(cluster_->Execute(pg::ClusterHostType::kMaster, sql::kLastRevision));
}

Revision ConfigStore::Current(pg::Transaction& transaction) const {
    return WithDefaults(transaction.Execute(sql::kLastRevision));
}

ConfigValues ConfigStore::Values(const std::optional<std::vector<std::string>>& ids) const {
    const auto current = Current();
    userver::formats::json::ValueBuilder values{userver::formats::json::Type::kObject};
    if (ids) {
        for (const auto& id : *ids)
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
    Validate(values);
    const auto published =
        transaction.Execute(sql::kInsertRevision, values).AsSingleRow<Revision>(pg::kRowTag);
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!current.Values.HasMember(it.GetName()) || current.Values[it.GetName()] != *it)
            transaction.Execute(sql::kSetConfigVersion, it.GetName(), published.Id);
    }
    return published;
}

std::vector<RevisionInfo> ConfigStore::History() const {
    return cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistory)
        .AsContainer<std::vector<RevisionInfo>>(pg::kRowTag);
}

}  // namespace configs
