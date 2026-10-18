#pragma once
#include <models/draft.hpp>
namespace configs::storage {
Draft ReadDraft(std::int64_t id, const Json& document);
Json WriteDraft(const Draft& draft);
std::string_view StatusName(DraftStatus status);
}  // namespace configs::storage
