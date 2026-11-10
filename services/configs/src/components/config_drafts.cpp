#include <algorithm>
#include <components/config_store.hpp>
#include <configs/keys.hpp>
#include <configs/sql_queries.hpp>
#include <converters/draft_document.hpp>
#include <converters/rows.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/regex.hpp>

namespace {
namespace pg = userver::storages::postgres;
using configs::Json;
using Builder = userver::formats::json::ValueBuilder;

void ValidateText(const std::string& value, std::size_t maximum) {
    const auto length = std::count_if(
        value.begin(), value.end(), [](unsigned char byte) { return (byte & 0xc0) != 0x80; });
    if (value.empty() || static_cast<std::size_t>(length) > maximum ||
        value.find_first_not_of(" \t\r\n") == std::string::npos)
        throw configs::InvalidRequest("Invalid text field");
}

void ValidateId(std::int64_t id) {
    if (id <= 0)
        throw configs::InvalidRequest("Invalid draft id");
}

void ValidateName(const std::string& name) {
    static const userver::utils::regex pattern{"[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*"};
    if (name == "BROKER_CONFIG_DELIVERY_META" || !userver::utils::regex_match(name, pattern))
        throw configs::InvalidRequest("Используйте имя в формате MY_CONFIG");
}
}  // namespace

namespace configs {
void ConfigStore::ValidateConfig(const std::string& name, const Json& value) const {
    ValidateName(name);
    if (!value.IsObject())
        throw InvalidRequest("Значение конфига должно быть JSON-объектом");
    // Для известных схем сохраняем проверку; неизвестные ключи проверяет типизированный
    // потребитель.
    if (!Defaults().HasMember(name))
        return;
    Builder patch{userver::formats::json::Type::kObject};
    patch[name] = value;
    try {
        configs::Validate(patch.ExtractValue());
    } catch (const std::invalid_argument& error) {
        throw InvalidRequest(error.what());
    } catch (const userver::formats::json::Exception& error) {
        throw InvalidRequest(error.what());
    }
}

std::vector<CatalogEntry> ConfigStore::Catalog() const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kCatalog);
    return converters::Convert(rows.AsContainer<std::vector<converters::CatalogRow>>(pg::kRowTag));
}

Config ConfigStore::GetConfig(const std::string& name) const {
    ValidateName(name);
    // Значение, версия и активный драфт читаются согласованно, без блокировки публикации.
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto current = Current(transaction);
    const auto version = transaction.Execute(sql::kConfigVersion, name).AsSingleRow<std::int64_t>();
    const auto active = transaction.Execute(sql::kActiveDraft, name);
    const auto draft_id =
        active.IsEmpty() ? std::nullopt : std::optional{active.AsSingleRow<std::int64_t>()};
    const auto value =
        current.Values.HasMember(name) ? std::optional{current.Values[name]} : std::nullopt;
    transaction.Commit();
    return {name, value, version, draft_id};
}

Draft ConfigStore::GetDraft(std::int64_t id) const {
    ValidateId(id);
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id);
    if (rows.IsEmpty())
        throw RevisionNotFound("Draft not found");
    return converters::ReadDraft(id, rows.AsSingleRow<Json>());
}

std::vector<Draft> ConfigStore::ListDrafts(const models::ListConfigDraftsRequest& filter) const {
    const auto name = filter.config_name.value_or("");
    const auto before = filter.before_id.value_or(0);
    if (!name.empty())
        ValidateName(name);
    if (before < 0)
        throw InvalidRequest("Invalid list filter");

    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListDrafts, name, before);
    return converters::Convert(rows.AsContainer<std::vector<converters::DraftRow>>(pg::kRowTag));
}

Draft ConfigStore::LoadActiveDraft(
    pg::Transaction& transaction, std::int64_t id, std::int64_t expected_version) const {
    ValidateId(id);
    const auto rows = transaction.Execute(sql::kFindDraft, id);
    if (rows.IsEmpty())
        throw RevisionNotFound("Draft not found");
    auto draft = converters::ReadDraft(id, rows.AsSingleRow<Json>());
    if (draft.Version != expected_version)
        throw RevisionConflict("Draft version changed");
    if (draft.Status != DraftStatus::Draft)
        throw RevisionConflict("Draft is closed");
    return draft;
}

void ConfigStore::SaveDraft(pg::Transaction& transaction, const Draft& draft,
    const std::string& actor, std::string_view action) const {
    const auto document = converters::WriteDraft(draft);
    transaction.Execute(sql::kUpdateDraft, draft.Id, document);
    WriteDraftEvent(transaction, draft, actor, action);
}

void ConfigStore::WriteDraftEvent(pg::Transaction& transaction, const Draft& draft,
    const std::string& actor, std::string_view action) const {
    const auto document = converters::WriteDraft(draft);
    transaction.Execute(
        sql::kInsertDraftEvent, draft.Id, converters::WriteDraftEvent(document, actor, action));
}

Draft ConfigStore::CreateDraft(const models::CreateConfigDraftRequest& request,
    const platform::auth::Identity& identity) const {
    ValidateText(identity.ActorId(), 2048);
    ValidateText(request.config_name, 256);
    ValidateText(request.description, 4096);
    ValidateName(request.config_name);
    if (static_cast<int>(request.value.has_value()) +
            static_cast<int>(request.source_revision.has_value()) +
            static_cast<int>(request.remove.value_or(false)) !=
        1)
        throw InvalidRequest("Выберите значение, историческую ревизию или удаление");
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Общая блокировка защищает проверку версии, единственность драфта и публикацию.
    transaction.Execute(sql::kLockRevision);
    if (!transaction.Execute(sql::kActiveDraft, request.config_name).IsEmpty())
        throw RevisionConflict("Config already has an active draft");
    const auto current = Current(transaction);
    const auto version =
        transaction.Execute(sql::kConfigVersion, request.config_name).AsSingleRow<std::int64_t>();
    if (request.expected_config_version != version)
        throw RevisionConflict("Config changed while editing");
    const auto base = current.Values.HasMember(request.config_name)
        ? std::optional{current.Values[request.config_name]}
        : std::nullopt;
    if (request.remove.value_or(false) && !base.has_value())
        throw RevisionNotFound("Конфиг не опубликован");
    std::optional<Json> value;
    if (request.value.has_value()) {
        value = request.value.value();
    } else if (request.source_revision.has_value()) {
        const auto source = request.source_revision.value();
        if (source < 0)
            throw InvalidRequest("Invalid source revision");
        const auto rows = transaction.Execute(sql::kFindRevision, source);
        if (rows.IsEmpty())
            throw RevisionNotFound("Revision not found");
        const auto snapshot = rows.AsSingleRow<Revision>(pg::kRowTag).Values;
        if (!snapshot.HasMember(request.config_name))
            throw RevisionNotFound("Config did not exist in revision");
        value = snapshot[request.config_name];
    }
    if (value.has_value())
        ValidateConfig(request.config_name, value.value());
    if (base.has_value() && value == base.value())
        throw InvalidRequest("Config value has no changes");
    Draft draft{.Id = 0,
        .ConfigName = request.config_name,
        .Author = identity.ActorId(),
        .Description = request.description,
        .Version = 1,
        .Status = DraftStatus::Draft,
        .ApprovedBy = std::nullopt,
        .BaseVersion = version,
        .BaseValue = base,
        .Value = value,
        .SourceRevision = request.source_revision,
        .PublishedRevision = std::nullopt,
        .AuthorName = identity.DisplayName,
        .ApprovedByName = std::nullopt};
    draft.Id = transaction.Execute(sql::kInsertDraft, converters::WriteDraft(draft))
                   .AsSingleRow<std::int64_t>();
    WriteDraftEvent(transaction, draft, identity.ActorId(), "create");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::EditDraft(
    const models::EditConfigDraftRequest& request, const platform::auth::Identity& identity) const {
    ValidateText(identity.ActorId(), 2048);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.id, request.expected_version);
    if (draft.Author != identity.ActorId())
        throw DraftForbidden("Only author can edit");
    if (!request.value.has_value() && !request.description.has_value())
        throw InvalidRequest("Empty edit");
    if (request.value.has_value()) {
        if (!draft.Value.has_value())
            throw InvalidRequest("Драфт удаления нельзя заменить изменением значения");
        ValidateConfig(draft.ConfigName, request.value.value());
        if (draft.BaseValue.has_value() && request.value.value() == draft.BaseValue.value())
            throw InvalidRequest("Config value has no changes");
        draft.Value = request.value.value();
    }
    if (request.description.has_value()) {
        ValidateText(request.description.value(), 4096);
        draft.Description = request.description.value();
    }
    ++draft.Version;
    SaveDraft(transaction, draft, identity.ActorId(), "edit");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::CancelDraft(const models::ConfigDraftActionRequest& request,
    const platform::auth::Identity& identity) const {
    ValidateText(identity.ActorId(), 2048);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.id, request.expected_version);
    if (draft.Author != identity.ActorId())
        throw DraftForbidden("Only author can cancel");
    draft.Status = DraftStatus::Cancelled;
    ++draft.Version;
    SaveDraft(transaction, draft, identity.ActorId(), "cancel");
    transaction.Commit();
    return draft;
}

Draft ConfigStore::ApproveDraft(const models::ConfigDraftActionRequest& request,
    const platform::auth::Identity& identity) const {
    ValidateText(identity.ActorId(), 2048);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockRevision);
    auto draft = LoadActiveDraft(transaction, request.id, request.expected_version);
    if (draft.Author == identity.ActorId() && !allow_self_approval_)
        throw DraftForbidden("Author cannot approve");
    if (draft.Value.has_value())
        ValidateConfig(draft.ConfigName, draft.Value.value());
    const auto current = Current(transaction);
    const auto version =
        transaction.Execute(sql::kConfigVersion, draft.ConfigName).AsSingleRow<std::int64_t>();
    const auto value = current.Values.HasMember(draft.ConfigName)
        ? std::optional{current.Values[draft.ConfigName]}
        : std::nullopt;
    if (draft.BaseVersion != version || draft.BaseValue != value)
        throw RevisionConflict("Config changed; cancel draft and open editor again");
    Builder values{current.Values};
    if (draft.Value.has_value())
        values[draft.ConfigName] = draft.Value.value();
    else
        values.Remove(draft.ConfigName);
    const auto published = PublishLocked(transaction, current, values.ExtractValue());
    draft.PublishedRevision = published.Id;
    draft.Status = DraftStatus::Published;
    draft.ApprovedBy = identity.ActorId();
    draft.ApprovedByName = identity.DisplayName;
    ++draft.Version;
    SaveDraft(transaction, draft, identity.ActorId(), "approve");
    transaction.Commit();
    LOG_INFO() << "Config revision published"
               << userver::logging::LogExtra{{"config_revision", published.Id}};
    return draft;
}

std::vector<DraftEvent> ConfigStore::DraftEvents(std::int64_t id) const {
    ValidateId(id);
    if (cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindDraft, id).IsEmpty())
        throw RevisionNotFound("Draft not found");

    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kDraftEvents, id);
    return converters::Convert(
        id, rows.AsContainer<std::vector<converters::EventRow>>(pg::kRowTag));
}
}  // namespace configs
