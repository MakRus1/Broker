#pragma once
#include <platform/handler_error.hpp>

namespace counterparties {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}  // namespace counterparties
