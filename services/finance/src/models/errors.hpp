#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace finance {
template <int Status> struct ApiError : std::runtime_error {
    static_assert(Status >= 400 && Status <= 599);
    std::string Code;

    ApiError(std::string code, const std::string& message)
        : std::runtime_error(message), Code(std::move(code)) {}
};

}  // namespace finance
