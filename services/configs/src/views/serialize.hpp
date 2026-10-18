#pragma once

#include <domain/models.hpp>

namespace configs::views {
// Вложенные модели пока не поддерживаются кодгеном: сериализуем элементы списков на HTTP-границе.
inline Json SerializeDraft(const Draft& draft) {
    userver::formats::json::ValueBuilder item;
    item["id"] = draft.Id;
    item["config_name"] = draft.ConfigName;
    item["author"] = draft.Author;
    item["description"] = draft.Description;
    item["version"] = draft.Version;
    switch (draft.Status) {
    case DraftStatus::Draft:
        item["status"] = "draft";
        break;
    case DraftStatus::Published:
        item["status"] = "published";
        break;
    case DraftStatus::Cancelled:
        item["status"] = "cancelled";
        break;
    }
    if (draft.ApprovedBy)
        item["approved_by"] = *draft.ApprovedBy;
    else
        item["approved_by"] = nullptr;
    item["base_version"] = draft.BaseVersion;
    item["base_value"] = draft.BaseValue;
    item["value"] = draft.Value;
    if (draft.SourceRevision)
        item["source_revision"] = *draft.SourceRevision;
    if (draft.PublishedRevision)
        item["published_revision"] = *draft.PublishedRevision;
    return item.ExtractValue();
}
inline std::vector<Json> SerializeDrafts(const std::vector<Draft>& drafts) {
    std::vector<Json> items;
    for (const auto& draft : drafts)
        items.push_back(SerializeDraft(draft));
    return items;
}
inline std::vector<Json> SerializeCatalog(const std::vector<CatalogEntry>& entries) {
    std::vector<Json> items;
    for (const auto& entry : entries) {
        userver::formats::json::ValueBuilder item;
        item["config_name"] = entry.Name;
        item["version"] = entry.Version;
        if (entry.ActiveDraftId)
            item["active_draft_id"] = *entry.ActiveDraftId;
        else
            item["active_draft_id"] = nullptr;
        items.push_back(item.ExtractValue());
    }
    return items;
}
inline std::vector<Json> SerializeHistory(const std::vector<RevisionInfo>& revisions) {
    std::vector<Json> items;
    for (const auto& revision : revisions) {
        userver::formats::json::ValueBuilder item;
        item["revision"] = revision.Id;
        item["created_at"] = revision.CreatedAt;
        items.push_back(item.ExtractValue());
    }
    return items;
}
inline std::vector<Json> SerializeDraftEvents(const std::vector<DraftEvent>& events) {
    std::vector<Json> items;
    for (const auto& event : events) {
        userver::formats::json::ValueBuilder item;
        item["action"] = event.Action;
        item["actor"] = event.Actor;
        auto snapshot = userver::formats::json::ValueBuilder{SerializeDraft(event.Snapshot)};
        snapshot.Remove("id");
        item["snapshot"] = snapshot.ExtractValue();
        item["created_at"] = event.CreatedAt;
        items.push_back(item.ExtractValue());
    }
    return items;
}
}  // namespace configs::views
