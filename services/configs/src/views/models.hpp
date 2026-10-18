#pragma once

#include <domain/models.hpp>
#include <openapi/models.hpp>

namespace configs::views {
inline models::DraftStatus ToApiStatus(DraftStatus status) {
    switch (status) {
    case DraftStatus::Draft:
        return models::DraftStatus::Draft;
    case DraftStatus::Published:
        return models::DraftStatus::Published;
    case DraftStatus::Cancelled:
        return models::DraftStatus::Cancelled;
    }
    throw std::logic_error("Invalid draft status");
}
inline models::ConfigDraft ToApiDraft(const Draft& draft) {
    return {.approved_by = draft.ApprovedBy,
            .author = draft.Author,
            .base_value = draft.BaseValue,
            .base_version = draft.BaseVersion,
            .config_name = draft.ConfigName,
            .description = draft.Description,
            .id = draft.Id,
            .published_revision = draft.PublishedRevision,
            .source_revision = draft.SourceRevision,
            .status = ToApiStatus(draft.Status),
            .value = draft.Value,
            .version = draft.Version};
}
inline models::DraftSnapshot ToApiSnapshot(const Draft& draft) {
    return {.approved_by = draft.ApprovedBy,
            .author = draft.Author,
            .base_value = draft.BaseValue,
            .base_version = draft.BaseVersion,
            .config_name = draft.ConfigName,
            .description = draft.Description,
            .published_revision = draft.PublishedRevision,
            .source_revision = draft.SourceRevision,
            .status = ToApiStatus(draft.Status),
            .value = draft.Value,
            .version = draft.Version};
}
inline std::vector<models::ConfigDraft> ToApiDrafts(const std::vector<Draft>& drafts) {
    std::vector<models::ConfigDraft> items;
    for (const auto& draft : drafts)
        items.push_back(ToApiDraft(draft));
    return items;
}
inline std::vector<models::ConfigCatalogEntry>
ToApiCatalog(const std::vector<CatalogEntry>& entries) {
    std::vector<models::ConfigCatalogEntry> items;
    for (const auto& entry : entries)
        items.push_back({.active_draft_id = entry.ActiveDraftId,
                         .config_name = entry.Name,
                         .version = entry.Version});
    return items;
}
inline std::vector<models::ConfigRevision>
ToApiHistory(const std::vector<RevisionInfo>& revisions) {
    std::vector<models::ConfigRevision> items;
    for (const auto& revision : revisions)
        items.push_back({.created_at = revision.CreatedAt, .revision = revision.Id});
    return items;
}
inline std::vector<models::DraftEvent> ToApiDraftEvents(const std::vector<DraftEvent>& events) {
    std::vector<models::DraftEvent> items;
    for (const auto& event : events)
        items.push_back({.action = event.Action,
                         .actor = event.Actor,
                         .created_at = event.CreatedAt,
                         .snapshot = ToApiSnapshot(event.Snapshot)});
    return items;
}
}  // namespace configs::views
