#include <algorithm>
#include <components/config_store.hpp>
#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace {
namespace pg = userver::storages::postgres;
using Json = platform::configs::Json;
using Builder = userver::formats::json::ValueBuilder;

std::string Text(const Json& request, std::string_view key, std::size_t maximum) {
    auto value = request[key].As<std::string>();
    const auto length = std::count_if(value.begin(), value.end(),
                                      [](unsigned char byte) { return (byte & 0xc0) != 0x80; });
    if (value.empty() || static_cast<std::size_t>(length) > maximum ||
        value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument("Invalid text field");
    return value;
}
Json Patch(const std::string& name, const Json& value) {
    Builder patch{userver::formats::json::Type::kObject};
    patch[name] = value;
    return patch.ExtractValue();
}
Json Document(pg::Transaction& transaction, std::int64_t id) {
    const auto rows = transaction.Execute(configs::sql::kFindDraft, id);
    if (rows.IsEmpty())
        throw configs::RevisionNotFound("Draft not found");
    return rows.AsSingleRow<Json>();
}
}  // namespace

namespace configs {

platform::configs::Json ConfigStore::Catalog() const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    struct Entry {
        std::string Name;
        std::int64_t Version;
        std::int64_t DraftId;
    };
    const auto rows = transaction.Execute(sql::kCatalog, Defaults());
    Builder items{userver::formats::json::Type::kArray};
    for (const auto& row : rows.AsSetOf<Entry>(pg::kRowTag)) {
        Builder item;
        item["config_name"] = row.Name;
        item["version"] = row.Version;
        if (row.DraftId)
            item["active_draft_id"] = row.DraftId;
        else
            item["active_draft_id"] = nullptr;
        items.PushBack(item.ExtractValue());
    }
    transaction.Commit();
    return items.ExtractValue();
}

platform::configs::Json ConfigStore::GetConfig(const std::string& name) const {
    if (!Defaults().HasMember(name))
        throw std::invalid_argument("Unknown config");
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    const auto rows = transaction.Execute(sql::kLastRevision);
    auto values = rows.IsEmpty() ? Defaults() : rows.AsSingleRow<Revision>(pg::kRowTag).Values;
    Builder result;
    result["config_name"] = name;
    result["value"] = values.HasMember(name) ? values[name] : Defaults()[name];
    result["version"] = transaction.Execute(sql::kConfigVersion, name).AsSingleRow<std::int64_t>();
    const auto active = transaction.Execute(sql::kActiveDraft, name);
    if (active.IsEmpty())
        result["active_draft_id"] = nullptr;
    else
        result["active_draft_id"] = active.AsSingleRow<std::int64_t>();
    transaction.Commit();
    return result.ExtractValue();
}

platform::configs::Json ConfigStore::Draft(std::string_view action, const Json& request) const {
    using platform::configs::ObjectKeys;
    if (action == "list") {
        ObjectKeys(request, {"config_name", "before_id"});
        const auto name = request["config_name"].As<std::string>("");
        const auto before = request["before_id"].As<std::int64_t>(0);
        if (before < 0 || (!name.empty() && !Defaults().HasMember(name)))
            throw std::invalid_argument("Invalid list filter");
        struct Row {
            std::int64_t Id;
            Json Document;
        };
        Builder items{userver::formats::json::Type::kArray};
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListDrafts, name, before);
        for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag)) {
            Builder item{row.Document};
            item["id"] = row.Id;
            items.PushBack(item.ExtractValue());
        }
        return items.ExtractValue();
    }
    if (action == "get") {
        ObjectKeys(request, {"id"});
        const auto id = request["id"].As<std::int64_t>();
        if (id <= 0)
            throw std::invalid_argument("Invalid draft id");
        const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id);
        if (rows.IsEmpty())
            throw RevisionNotFound("Draft not found");
        Builder result{rows.AsSingleRow<Json>()};
        result["id"] = id;
        return result.ExtractValue();
    }
    if (action == "create")
        ObjectKeys(request, {"actor", "config_name", "description", "value", "source_revision",
                             "expected_config_version"});
    else if (action == "edit")
        ObjectKeys(request, {"actor", "id", "expected_version", "value", "description"});
    else
        ObjectKeys(request, {"actor", "id", "expected_version"});
    const auto actor = Text(request, "actor", 128);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Тот же lock используют прямые публикации: согласование не допускает гонок с ними.
    transaction.Execute(sql::kLockRevision);
    const auto current_rows = transaction.Execute(sql::kLastRevision);
    auto current = current_rows.IsEmpty() ? Revision{0, Defaults()}
                                          : current_rows.AsSingleRow<Revision>(pg::kRowTag);
    Builder current_values{Defaults()};
    for (auto it = current.Values.begin(); it != current.Values.end(); ++it)
        current_values[it.GetName()] = *it;
    current.Values = current_values.ExtractValue();
    std::int64_t id;
    Builder draft;
    if (action == "create") {
        const auto name = Text(request, "config_name", 256);
        if (!Defaults().HasMember(name))
            throw std::invalid_argument("Unknown config");
        if (!transaction.Execute(sql::kActiveDraft, name).IsEmpty())
            throw RevisionConflict("Config already has an active draft");
        if (request.HasMember("value") == request.HasMember("source_revision"))
            throw std::invalid_argument("Choose value or source_revision");
        draft["config_name"] = name;
        draft["author"] = actor;
        draft["description"] = Text(request, "description", 4096);
        draft["version"] = 1;
        draft["status"] = "draft";
        draft["approved_by"] = nullptr;
        const auto config_version =
            transaction.Execute(sql::kConfigVersion, name).AsSingleRow<std::int64_t>();
        if (request["expected_config_version"].As<std::int64_t>() != config_version)
            throw RevisionConflict("Config changed while editing");
        draft["base_version"] = config_version;
        draft["base_value"] = current.Values[name];
        if (request.HasMember("value")) {
            draft["value"] = request["value"];
        } else {
            const auto source = request["source_revision"].As<std::int64_t>();
            if (source < 0)
                throw std::invalid_argument("Invalid source revision");
            auto values = Defaults();
            if (source != 0) {
                const auto rows = transaction.Execute(sql::kFindRevision, source);
                if (rows.IsEmpty())
                    throw RevisionNotFound("Revision not found");
                values = rows.AsSingleRow<Revision>(pg::kRowTag).Values;
            }
            draft["value"] = values.HasMember(name) ? values[name] : Defaults()[name];
            draft["source_revision"] = source;
        }
        const auto document = draft.ExtractValue();
        Validate(Patch(name, document["value"]));
        id = transaction.Execute(sql::kInsertDraft, document).AsSingleRow<std::int64_t>();
        draft = Builder{Document(transaction, id)};
    } else {
        id = request["id"].As<std::int64_t>();
        if (id <= 0)
            throw std::invalid_argument("Invalid draft id");
        const auto old = Document(transaction, id);
        const auto version = old["version"].As<std::int64_t>();
        if (request["expected_version"].As<std::int64_t>() != version)
            throw RevisionConflict("Draft version changed");
        const auto state = old["status"].As<std::string>();
        if (state == "published" || state == "cancelled")
            throw RevisionConflict("Draft is closed");
        const auto name = old["config_name"].As<std::string>();
        draft = Builder{old};
        if (action == "edit") {
            if (old["author"].As<std::string>() != actor)
                throw DraftForbidden("Only author can edit");
            if (!request.HasMember("value") && !request.HasMember("description"))
                throw std::invalid_argument("Empty edit");
            if (request.HasMember("value")) {
                Validate(Patch(name, request["value"]));
                draft["value"] = request["value"];
            }
            if (request.HasMember("description"))
                draft["description"] = Text(request, "description", 4096);
        } else if (action == "cancel") {
            if (old["author"].As<std::string>() != actor)
                throw DraftForbidden("Only author can cancel");
            draft["status"] = "cancelled";
        } else if (action == "approve") {
            if (state != "draft")
                throw RevisionConflict("Expected active draft");
            if (old["author"].As<std::string>() == actor)
                throw DraftForbidden("Author cannot approve");
            Validate(Patch(name, old["value"]));
            if (old["base_version"].As<std::int64_t>() !=
                    transaction.Execute(sql::kConfigVersion, name).AsSingleRow<std::int64_t>() ||
                old["base_value"] != current.Values[name])
                throw RevisionConflict("Config changed; cancel draft and open editor again");
            Builder values{current.Values};
            values[name] = old["value"];
            const auto published = PublishLocked(transaction, current, values.ExtractValue());
            draft["published_revision"] = published.Id;
            draft["status"] = "published";
            draft["approved_by"] = actor;
        } else {
            throw std::invalid_argument("Unknown draft action");
        }
        draft["version"] = version + 1;
    }
    const auto document = draft.ExtractValue();
    transaction.Execute(sql::kUpdateDraft, id, document);
    Builder event;
    event["action"] = std::string(action);
    event["actor"] = actor;
    event["snapshot"] = document;
    transaction.Execute(sql::kInsertDraftEvent, id, event.ExtractValue());
    transaction.Commit();
    if (action == "approve")
        LOG_INFO() << "Config revision published"
                   << userver::logging::LogExtra{
                          {"config_revision", document["published_revision"].As<std::int64_t>()}};
    Builder result{document};
    result["id"] = id;
    return result.ExtractValue();
}

platform::configs::Json ConfigStore::DraftEvents(std::int64_t id) const {
    if (id <= 0)
        throw std::invalid_argument("Invalid draft id");
    if (cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id).IsEmpty())
        throw RevisionNotFound("Draft not found");
    struct Row {
        Json Event;
        std::string Created;
    };
    Builder result{userver::formats::json::Type::kArray};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kDraftEvents, id);
    for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag)) {
        Builder item{row.Event};
        item["created_at"] = row.Created;
        result.PushBack(item.ExtractValue());
    }
    return result.ExtractValue();
}
}  // namespace configs
