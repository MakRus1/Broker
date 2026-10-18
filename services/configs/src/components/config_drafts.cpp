#include <algorithm>
#include <components/config_store.hpp>
#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <repositories/draft_document.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace {
namespace pg = userver::storages::postgres;
using configs::Json;
using Builder = userver::formats::json::ValueBuilder;

void ValidateText(const std::string& value, std::size_t maximum) {
    const auto length = std::count_if(value.begin(), value.end(),
                                      [](unsigned char byte) { return (byte & 0xc0) != 0x80; });
    if (value.empty() || static_cast<std::size_t>(length) > maximum ||
        value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw configs::InvalidRequest("Invalid text field");
}
void ValidateId(std::int64_t id) {
    if (id <= 0)
        throw configs::InvalidRequest("Invalid draft id");
}
void ValidateConfig(const std::string& name, const Json& value) {
    Builder patch{userver::formats::json::Type::kObject};
    patch[name] = value;
    // Только ошибки проверки входного значения считаются ошибками клиента.
    try {
        configs::Validate(patch.ExtractValue());
    } catch (const std::invalid_argument& error) {
        throw configs::InvalidRequest(error.what());
    } catch (const userver::formats::json::Exception& error) {
        throw configs::InvalidRequest(error.what());
    }
}
}  // namespace

namespace configs {
std::vector<CatalogEntry> ConfigStore::Catalog() const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    struct Row {
        std::string Name;
        std::int64_t Version;
        std::int64_t DraftId;
    };
    const auto rows = transaction.Execute(sql::kCatalog, Defaults());
    std::vector<CatalogEntry> result;
    for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag))
        result.push_back(
            {row.Name, row.Version, row.DraftId ? std::optional{row.DraftId} : std::nullopt});
    transaction.Commit();
    return result;
}

Config ConfigStore::GetConfig(const std::string& name) const {
    if (!Defaults().HasMember(name))
        throw InvalidRequest("Unknown config");
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    const auto current = Current(transaction);
    const auto version = transaction.Execute(sql::kConfigVersion, name).AsSingleRow<std::int64_t>();
    const auto active = transaction.Execute(sql::kActiveDraft, name);
    const auto draft_id =
        active.IsEmpty() ? std::nullopt : std::optional{active.AsSingleRow<std::int64_t>()};
    transaction.Commit();
    return {name, current.Values[name], version, draft_id};
}

Draft ConfigStore::GetDraft(std::int64_t id) const {
    ValidateId(id);
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id);
    if (rows.IsEmpty())
        throw RevisionNotFound("Draft not found");
    return storage::ReadDraft(id, rows.AsSingleRow<Json>());
}

std::vector<Draft> ConfigStore::ListDrafts(const DraftFilter& filter) const {
    const auto name = filter.ConfigName.value_or("");
    const auto before = filter.BeforeId.value_or(0);
    if (before < 0 || (!name.empty() && !Defaults().HasMember(name)))
        throw InvalidRequest("Invalid list filter");
    struct Row {
        std::int64_t Id;
        Json Document;
    };
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListDrafts, name, before);
    std::vector<Draft> result;
    for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag))
        result.push_back(storage::ReadDraft(row.Id, row.Document));
    return result;
}

Draft ConfigStore::LoadActiveDraft(pg::Transaction& transaction, std::int64_t id,
                                   std::int64_t expected_version) const {
    ValidateId(id);
    const auto rows = transaction.Execute(sql::kFindDraft, id);
    if (rows.IsEmpty())
        throw RevisionNotFound("Draft not found");
    auto draft = storage::ReadDraft(id, rows.AsSingleRow<Json>());
    if (draft.Version != expected_version)
        throw RevisionConflict("Draft version changed");
    if (draft.Status != DraftStatus::Draft)
        throw RevisionConflict("Draft is closed");
    return draft;
}

void ConfigStore::SaveDraft(pg::Transaction& transaction, const Draft& draft,
                            const std::string& actor, std::string_view action) const {
    const auto document = storage::WriteDraft(draft);
    transaction.Execute(sql::kUpdateDraft, draft.Id, document);
    Builder event;
    event["action"] = std::string(action);
    event["actor"] = actor;
    event["snapshot"] = document;
    transaction.Execute(sql::kInsertDraftEvent, draft.Id, event.ExtractValue());
}

Draft ConfigStore::CreateDraft(const CreateDraftRequest& request) const {
    ValidateText(request.Actor, 128);
    ValidateText(request.ConfigName, 256);
    ValidateText(request.Description, 4096);
    if (!Defaults().HasMember(request.ConfigName))
        throw InvalidRequest("Unknown config");
    if (request.Value.has_value() == request.SourceRevision.has_value())
        throw InvalidRequest("Choose value or source_revision");
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Общая блокировка защищает проверку версии, единственность драфта и публикацию.
    transaction.Execute(sql::kLockRevision);
    if (!transaction.Execute(sql::kActiveDraft, request.ConfigName).IsEmpty())
        throw RevisionConflict("Config already has an active draft");
    const auto current = Current(transaction);
    const auto version =
        transaction.Execute(sql::kConfigVersion, request.ConfigName).AsSingleRow<std::int64_t>();
    if (request.ExpectedConfigVersion != version)
        throw RevisionConflict("Config changed while editing");
    Json value;
    if (request.Value) {
        value = *request.Value;
    } else {
        const auto source = *request.SourceRevision;
        if (source < 0)
            throw InvalidRequest("Invalid source revision");
        auto values = Defaults();
        if (source != 0) {
            const auto rows = transaction.Execute(sql::kFindRevision, source);
            if (rows.IsEmpty())
                throw RevisionNotFound("Revision not found");
            values = rows.AsSingleRow<Revision>(pg::kRowTag).Values;
        }
        value = values.HasMember(request.ConfigName) ? values[request.ConfigName]
                                                     : Defaults()[request.ConfigName];
    }
    ValidateConfig(request.ConfigName, value);
    Draft draft{.Id = 0,
                .ConfigName = request.ConfigName,
                .Author = request.Actor,
                .Description = request.Description,
                .Version = 1,
                .Status = DraftStatus::Draft,
                .ApprovedBy = std::nullopt,
                .BaseVersion = version,
                .BaseValue = current.Values[request.ConfigName],
                .Value = value,
                .SourceRevision = request.SourceRevision,
                .PublishedRevision = std::nullopt};
    draft.Id = transaction.Execute(sql::kInsertDraft, storage::WriteDraft(draft))
                   .AsSingleRow<std::int64_t>();
    SaveDraft(transaction, draft, request.Actor, "create");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::EditDraft(const EditDraftRequest& request) const {
    ValidateText(request.Actor, 128);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.Id, request.ExpectedVersion);
    if (draft.Author != request.Actor)
        throw DraftForbidden("Only author can edit");
    if (!request.Value && !request.Description)
        throw InvalidRequest("Empty edit");
    if (request.Value) {
        ValidateConfig(draft.ConfigName, *request.Value);
        draft.Value = *request.Value;
    }
    if (request.Description) {
        ValidateText(*request.Description, 4096);
        draft.Description = *request.Description;
    }
    ++draft.Version;
    SaveDraft(transaction, draft, request.Actor, "edit");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::CancelDraft(const DraftActionRequest& request) const {
    ValidateText(request.Actor, 128);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.Id, request.ExpectedVersion);
    if (draft.Author != request.Actor)
        throw DraftForbidden("Only author can cancel");
    draft.Status = DraftStatus::Cancelled;
    ++draft.Version;
    SaveDraft(transaction, draft, request.Actor, "cancel");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::ApproveDraft(const DraftActionRequest& request) const {
    ValidateText(request.Actor, 128);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.Id, request.ExpectedVersion);
    if (draft.Author == request.Actor)
        throw DraftForbidden("Author cannot approve");
    ValidateConfig(draft.ConfigName, draft.Value);
    const auto current = Current(transaction);
    const auto version =
        transaction.Execute(sql::kConfigVersion, draft.ConfigName).AsSingleRow<std::int64_t>();
    if (draft.BaseVersion != version || draft.BaseValue != current.Values[draft.ConfigName])
        throw RevisionConflict("Config changed; cancel draft and open editor again");
    Builder values{current.Values};
    values[draft.ConfigName] = draft.Value;
    const auto published = PublishLocked(transaction, current, values.ExtractValue());
    draft.PublishedRevision = published.Id;
    draft.Status = DraftStatus::Published;
    draft.ApprovedBy = request.Actor;
    ++draft.Version;
    SaveDraft(transaction, draft, request.Actor, "approve");
    transaction.Commit();
    LOG_INFO() << "Config revision published"
               << userver::logging::LogExtra{{"config_revision", published.Id}};
    return draft;
}

std::vector<DraftEvent> ConfigStore::DraftEvents(std::int64_t id) const {
    ValidateId(id);
    if (cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id).IsEmpty())
        throw RevisionNotFound("Draft not found");
    struct Row {
        Json Event;
        std::string Created;
    };
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kDraftEvents, id);
    std::vector<DraftEvent> result;
    for (const auto& row : rows.AsSetOf<Row>(pg::kRowTag))
        result.push_back({row.Event["action"].As<std::string>(),
                          row.Event["actor"].As<std::string>(),
                          storage::ReadDraft(id, row.Event["snapshot"]), row.Created});
    return result;
}
}  // namespace configs
