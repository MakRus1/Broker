#pragma once
#include <models/draft.hpp>

namespace configs::converters {
Draft ReadDraft(std::int64_t id, const Json& document);
Json WriteDraft(const Draft& draft);
Json WriteDraftEvent(const Json& document, const std::string& actor, std::string_view action);
std::string_view StatusName(DraftStatus status);
}  // namespace configs::converters
