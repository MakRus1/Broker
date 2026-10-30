#pragma once
#include <platform/handler_error.hpp>

namespace contracts {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace contracts
