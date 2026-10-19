#pragma once

#include <clients/config_admin/client.hpp>
#include <openapi/models.hpp>

namespace admin_bff {
inline models::DraftStatus ToBrowserStatus(clients::config_admin::models::DraftStatus status) {
    using Status = clients::config_admin::models::DraftStatus;
    switch (status) {
    case Status::Draft:
        return models::DraftStatus::Draft;
    case Status::Published:
        return models::DraftStatus::Published;
    case Status::Cancelled:
        return models::DraftStatus::Cancelled;
    }
    throw std::logic_error("Invalid config draft status");
}

template <typename Response, typename Draft> Response ToBrowserDraft(const Draft& draft) {
    return {.approved_by = draft.approved_by,
        .approved_by_name = draft.approved_by_name,
        .author = draft.author,
        .author_name = draft.author_name,
        .base_value = draft.base_value,
        .base_version = draft.base_version,
        .config_name = draft.config_name,
        .description = draft.description,
        .id = draft.id,
        .published_revision = draft.published_revision,
        .source_revision = draft.source_revision,
        .status = ToBrowserStatus(draft.status),
        .value = draft.value,
        .version = draft.version};
}

inline models::DraftSnapshot ToBrowserSnapshot(
    const clients::config_admin::models::DraftSnapshot& draft) {
    return {.approved_by = draft.approved_by,
        .approved_by_name = draft.approved_by_name,
        .author = draft.author,
        .author_name = draft.author_name,
        .base_value = draft.base_value,
        .base_version = draft.base_version,
        .config_name = draft.config_name,
        .description = draft.description,
        .published_revision = draft.published_revision,
        .source_revision = draft.source_revision,
        .status = ToBrowserStatus(draft.status),
        .value = draft.value,
        .version = draft.version};
}
}  // namespace admin_bff
