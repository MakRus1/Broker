#pragma once
#include <stdexcept>
#include <string>

namespace organizations {
class ApiError : public std::runtime_error {
  public:
    ApiError(int status, std::string code, std::string message)
        : std::runtime_error(std::move(message)), Status(status), Code(std::move(code)) {}

    int Status;
    std::string Code;
};
}  // namespace organizations
