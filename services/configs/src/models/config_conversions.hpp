#pragma once

#include <models/config.hpp>
#include <openapi/models.hpp>
#include <vector>

namespace configs {
inline std::vector<models::ConfigCatalogEntry>
ToApiCatalog(const std::vector<CatalogEntry>& entries) {
    std::vector<models::ConfigCatalogEntry> items;
    for (const auto& entry : entries)
        items.push_back({.active_draft_id = entry.ActiveDraftId,
                         .config_name = entry.Name,
                         .version = entry.Version});
    return items;
}
}  // namespace configs
