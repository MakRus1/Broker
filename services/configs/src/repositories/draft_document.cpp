#include <repositories/draft_document.hpp>

namespace {
template <typename T>
std::optional<T> Optional(const configs::Json& document, std::string_view key) {
    if (!document.HasMember(key) || document[key].IsNull())
        return std::nullopt;
    return document[key].As<T>();
}

configs::DraftStatus ReadStatus(const std::string& value) {
    if (value == "draft")
        return configs::DraftStatus::Draft;
    if (value == "published")
        return configs::DraftStatus::Published;
    if (value == "cancelled")
        return configs::DraftStatus::Cancelled;
    throw std::runtime_error("Invalid stored draft status");
}
}  // namespace

namespace configs::storage {
std::string_view StatusName(DraftStatus status) {
    switch (status) {
    case DraftStatus::Draft:
        return "draft";
    case DraftStatus::Published:
        return "published";
    case DraftStatus::Cancelled:
        return "cancelled";
    }
    throw std::runtime_error("Invalid draft status");
}

Draft ReadDraft(std::int64_t id, const Json& document) {
    if (!document.HasMember("value"))
        throw std::runtime_error("Stored draft has no value");
    return {.Id = id,
        .ConfigName = document["config_name"].As<std::string>(),
        .Author = document["author"].As<std::string>(),
        .Description = document["description"].As<std::string>(),
        .Version = document["version"].As<std::int64_t>(),
        .Status = ReadStatus(document["status"].As<std::string>()),
        .ApprovedBy = Optional<std::string>(document, "approved_by"),
        .BaseVersion = document["base_version"].As<std::int64_t>(),
        .BaseValue = Optional<Json>(document, "base_value"),
        .Value = Optional<Json>(document, "value"),
        .SourceRevision = Optional<std::int64_t>(document, "source_revision"),
        .PublishedRevision = Optional<std::int64_t>(document, "published_revision"),
        .AuthorName = Optional<std::string>(document, "author_name"),
        .ApprovedByName = Optional<std::string>(document, "approved_by_name")};
}

Json WriteDraft(const Draft& draft) {
    userver::formats::json::ValueBuilder document;
    document["config_name"] = draft.ConfigName;
    document["author"] = draft.Author;
    if (draft.AuthorName.has_value())
        document["author_name"] = draft.AuthorName.value();
    if (draft.ApprovedByName.has_value())
        document["approved_by_name"] = draft.ApprovedByName.value();
    document["description"] = draft.Description;
    document["version"] = draft.Version;
    document["status"] = std::string(StatusName(draft.Status));
    if (draft.ApprovedBy.has_value())
        document["approved_by"] = draft.ApprovedBy.value();
    else
        document["approved_by"] = nullptr;
    document["base_version"] = draft.BaseVersion;
    if (draft.BaseValue.has_value())
        document["base_value"] = draft.BaseValue.value();
    else
        document["base_value"] = nullptr;
    if (draft.Value.has_value())
        document["value"] = draft.Value.value();
    else
        document["value"] = nullptr;
    if (draft.SourceRevision.has_value())
        document["source_revision"] = draft.SourceRevision.value();
    if (draft.PublishedRevision.has_value())
        document["published_revision"] = draft.PublishedRevision.value();
    return document.ExtractValue();
}
}  // namespace configs::storage
