#pragma once

#include <models/revision.hpp>
#include <openapi/models.hpp>
#include <vector>

namespace configs {
inline std::vector<models::ConfigRevision>
ToApiHistory(const std::vector<RevisionInfo>& revisions) {
    std::vector<models::ConfigRevision> items;
    for (const auto& revision : revisions)
        items.push_back({.created_at = revision.CreatedAt, .revision = revision.Id});
    return items;
}
}  // namespace configs
