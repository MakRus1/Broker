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
    return {.Id = id,
            .ConfigName = document["config_name"].As<std::string>(),
            .Author = document["author"].As<std::string>(),
            .Description = document["description"].As<std::string>(),
            .Version = document["version"].As<std::int64_t>(),
            .Status = ReadStatus(document["status"].As<std::string>()),
            .ApprovedBy = Optional<std::string>(document, "approved_by"),
            .BaseVersion = document["base_version"].As<std::int64_t>(),
            .BaseValue = document["base_value"],
            .Value = document["value"],
            .SourceRevision = Optional<std::int64_t>(document, "source_revision"),
            .PublishedRevision = Optional<std::int64_t>(document, "published_revision"),
            .AuthorName = Optional<std::string>(document, "author_name"),
            .ApprovedByName = Optional<std::string>(document, "approved_by_name")};
}
Json WriteDraft(const Draft& draft) {
    userver::formats::json::ValueBuilder document;
    document["config_name"] = draft.ConfigName;
    document["author"] = draft.Author;
    if (draft.AuthorName)
        document["author_name"] = *draft.AuthorName;
    if (draft.ApprovedByName)
        document["approved_by_name"] = *draft.ApprovedByName;
    document["description"] = draft.Description;
    document["version"] = draft.Version;
    document["status"] = std::string(StatusName(draft.Status));
    if (draft.ApprovedBy)
        document["approved_by"] = *draft.ApprovedBy;
    else
        document["approved_by"] = nullptr;
    document["base_version"] = draft.BaseVersion;
    document["base_value"] = draft.BaseValue;
    document["value"] = draft.Value;
    if (draft.SourceRevision)
        document["source_revision"] = *draft.SourceRevision;
    if (draft.PublishedRevision)
        document["published_revision"] = *draft.PublishedRevision;
    return document.ExtractValue();
}
}  // namespace configs::storage
