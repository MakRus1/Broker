#pragma once

#include <converters/draft.hpp>
#include <models/event.hpp>
#include <openapi/models.hpp>
#include <vector>

namespace configs::converters {
inline std::vector<models::DraftEvent> ToApiDraftEvents(const std::vector<DraftEvent>& events) {
    std::vector<models::DraftEvent> items;
    for (const auto& event : events)
        items.push_back({.action = event.Action,
            .actor = event.Actor,
            .created_at = event.CreatedAt,
            .snapshot = ToApiSnapshot(event.Snapshot)});
    return items;
}
}  // namespace configs::converters
