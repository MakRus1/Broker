#pragma once
#include <platform/handler_error.hpp>

namespace transportations {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace transportations
