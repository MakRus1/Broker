#pragma once
#include <models/config.hpp>
#include <models/event.hpp>
#include <repositories/draft_document.hpp>
#include <vector>

namespace configs::helpers {
struct CatalogRow {
    std::string Name;
    std::int64_t Version;
    std::int64_t DraftId;
};

struct DraftRow {
    std::int64_t Id;
    Json Document;
};

struct EventRow {
    Json Event;
    std::string Created;
};

inline std::vector<CatalogEntry> Convert(const std::vector<CatalogRow>& rows) {
    std::vector<CatalogEntry> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(
            {row.Name, row.Version, row.DraftId ? std::optional{row.DraftId} : std::nullopt});
    return result;
}

inline std::vector<Draft> Convert(const std::vector<DraftRow>& rows) {
    std::vector<Draft> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(storage::ReadDraft(row.Id, row.Document));
    return result;
}

inline std::vector<DraftEvent> Convert(std::int64_t id, const std::vector<EventRow>& rows) {
    std::vector<DraftEvent> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(
            {row.Event["action"].As<std::string>(), row.Event["actor"].As<std::string>(),
                storage::ReadDraft(id, row.Event["snapshot"]), row.Created});
    return result;
}
}  // namespace configs::helpers
