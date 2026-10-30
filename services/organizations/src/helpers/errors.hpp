#pragma once
#include <platform/handler_error.hpp>

namespace organizations {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace organizations
