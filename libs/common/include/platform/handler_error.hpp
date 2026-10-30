#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace platform::http {
// Ошибка сервиса, которую HTTP-handler преобразует только в объявленный ответ API.
template <int Status> class HandlerError : public std::runtime_error {
  public:
    static_assert(Status >= 400 && Status <= 599);
    std::string Code;

    HandlerError(std::string code, const std::string& message)
        : std::runtime_error(message), Code(std::move(code)) {}
};
}  // namespace platform::http
