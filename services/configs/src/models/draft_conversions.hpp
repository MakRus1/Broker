#pragma once

#include <models/draft.hpp>
#include <openapi/models.hpp>
#include <vector>

namespace configs {
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
}  // namespace configs
