#include <components/config_store.hpp>

#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <deps.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace configs {
namespace pg = userver::storages::postgres;

namespace {
Revision WithDefaults(const pg::ResultSet& rows) {
    auto defaults = Defaults();
    if (rows.IsEmpty())
        return {0, defaults};
    auto revision = rows.AsSingleRow<Revision>(pg::kRowTag);
    userver::formats::json::ValueBuilder merged{defaults};
    for (auto it = revision.Values.begin(); it != revision.Values.end(); ++it)
        merged[it.GetName()] = *it;
    revision.Values = merged.ExtractValue();
    return revision;
}
}  // namespace

ConfigStore::ConfigStore(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : ComponentBase(config, context), cluster_(ResolveDeps(context).pg_cluster) {}

Revision ConfigStore::Current() const {
    return WithDefaults(cluster_->Execute(pg::ClusterHostType::kMaster, sql::kLastRevision));
}

Revision ConfigStore::Publish(std::int64_t expected, const platform::configs::Json& patch,
                              std::optional<std::int64_t> rollback) const {
    if (expected < 0)
        throw std::invalid_argument("Invalid expected revision");
    Validate(patch);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Единый lock защищает проверку ревизии и публикацию от гонок между экземплярами сервиса.
    transaction.Execute(sql::kLockRevision);
    const auto current = WithDefaults(transaction.Execute(sql::kLastRevision));
    if (current.Id != expected)
        throw RevisionConflict("Config revision changed");
    userver::formats::json::ValueBuilder merged{current.Values};
    if (rollback) {
        const auto rows = transaction.Execute(sql::kFindRevision, *rollback);
        if (rows.IsEmpty())
            throw RevisionNotFound("Config revision not found");
        merged = userver::formats::json::ValueBuilder{WithDefaults(rows).Values};
    } else {
        for (auto it = patch.begin(); it != patch.end(); ++it)
            merged[it.GetName()] = *it;
    }
    const auto values = merged.ExtractValue();
    // Откат тоже проверяется по текущим схемам: история не обходит валидацию.
    Validate(values);
    const auto revision =
        transaction.Execute(sql::kInsertRevision, values).AsSingleRow<std::int64_t>();
    transaction.Commit();
    return {revision, values};
}

platform::configs::Json ConfigStore::History() const {
    struct Record {
        std::int64_t Id;
        std::string Created;
    };
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistory);
    userver::formats::json::ValueBuilder result{userver::formats::json::Type::kArray};
    for (const auto& row : rows.AsSetOf<Record>(pg::kRowTag)) {
        userver::formats::json::ValueBuilder item;
        item["revision"] = row.Id;
        item["created_at"] = row.Created;
        result.PushBack(item.ExtractValue());
    }
    return result.ExtractValue();
}

}  // namespace configs
