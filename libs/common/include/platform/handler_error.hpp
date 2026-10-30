#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace platform::http {
// Общая категория для сохранения ошибок при обработке зависимостей.
class HandlerErrorBase : public std::runtime_error {
  public:
    std::string Code;

  protected:
    HandlerErrorBase(std::string code, const std::string& message)
        : std::runtime_error(message), Code(std::move(code)) {}
};

// Ошибка сервиса, которую HTTP-handler преобразует только в объявленный ответ API.
template <int Status> class HandlerError : public HandlerErrorBase {
  public:
    static_assert(Status >= 400 && Status <= 599);

    HandlerError(std::string code, const std::string& message)
        : HandlerErrorBase(std::move(code), message) {}
};
}  // namespace platform::http
