#pragma once
#include <cstdint>
#include <models/errors.hpp>

namespace transportations::helpers {
[[noreturn]] inline void RevisionConflict() {
    throw ApiError<409>{
        "revision_conflict", "Перевозка изменилась. Обновите карточку и повторите сохранение"};
}

inline void CheckRevision(std::int64_t actual, std::int64_t expected) {
    if (actual != expected)
        RevisionConflict();
}
}  // namespace transportations::helpers
