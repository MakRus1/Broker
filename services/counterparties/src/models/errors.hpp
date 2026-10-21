#pragma once
#include <stdexcept>
#include <string>

namespace counterparties {
struct ApiError : std::runtime_error {
    int Status;
    std::string Code;

    ApiError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), Status(status), Code(std::move(code)) {}
};

}  // namespace counterparties
