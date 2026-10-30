#pragma once
#include <platform/handler_error.hpp>

namespace shipments {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace shipments
