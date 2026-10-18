#pragma once

#include <models/draft.hpp>

namespace configs {
struct DraftEvent {
    std::string Action;
    std::string Actor;
    Draft Snapshot;
    std::string CreatedAt;
};
}  // namespace configs
