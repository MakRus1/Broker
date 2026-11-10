#pragma once
#include <platform/handler_error.hpp>

namespace transport_locations {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace transport_locations
