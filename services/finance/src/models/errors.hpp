#pragma once
#include <platform/handler_error.hpp>

namespace finance {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace finance
