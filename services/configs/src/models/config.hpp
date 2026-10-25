#pragma once

#include <models/json.hpp>
#include <optional>
#include <string>

namespace configs {
struct ConfigValues {
    Json Values;
    std::int64_t Revision;
    // Протокол динамических конфигов использует updated_at как курсор ревизии, не дату.
    std::string RevisionCursor;
};

struct CatalogEntry {
    std::string Name;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};

struct Config {
    std::string Name;
    Json Value;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};
}  // namespace configs
