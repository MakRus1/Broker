#pragma once
#include <platform/handler_error.hpp>

namespace transportations {
template <int Status> using ApiError = platform::http::HandlerError<Status>;

class RevisionConflict final : public ApiError<409> {
  public:
    RevisionConflict()
        : ApiError<409>{"revision_conflict",
              "Перевозка изменилась. Обновите карточку и повторите сохранение"} {}
};
}  // namespace transportations
