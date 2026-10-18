#include <components/config_store.hpp>
#include <iomanip>
#include <sstream>

#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace {
namespace pg = userver::storages::postgres;
configs::Revision WithDefaults(const pg::ResultSet& rows) {
    auto defaults = configs::Defaults();
    if (rows.IsEmpty())
        return {0, defaults};
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

platform::configs::Json
ConfigStore::Values(const std::optional<std::vector<std::string>>& ids) const {
    const auto current = Current();
    userver::formats::json::ValueBuilder reply;
    userver::formats::json::ValueBuilder values{userver::formats::json::Type::kObject};
    if (ids) {
        for (const auto& id : *ids)
            if (current.Values.HasMember(id))
                values[id] = current.Values[id];
    } else
        values = userver::formats::json::ValueBuilder{current.Values};
    reply["configs"] = values.ExtractValue();
    std::ostringstream version;
    version << std::setw(20) << std::setfill('0') << current.Id;
    reply["updated_at"] = version.str();
    reply["revision"] = current.Id;
    return reply.ExtractValue();
}

Revision ConfigStore::PublishLocked(pg::Transaction& transaction, const Revision& current,
                                    const platform::configs::Json& values) const {
    // Откат и подтверждение тоже проверяются по текущим схемам.
    Validate(values);
    const auto revision =
        transaction.Execute(sql::kInsertRevision, values).AsSingleRow<std::int64_t>();
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!current.Values.HasMember(it.GetName()) || current.Values[it.GetName()] != *it)
            transaction.Execute(sql::kSetConfigVersion, it.GetName(), revision);
    }
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
