#pragma once
#include <platform/handler_error.hpp>

namespace sales {
template <int Status> using ApiError = platform::http::HandlerError<Status>;
}
