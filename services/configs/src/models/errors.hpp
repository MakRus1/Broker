#pragma once
#include <platform/handler_error.hpp>

namespace configs {
class InvalidRequest : public platform::http::HandlerError<400> {
  public:
    explicit InvalidRequest(const std::string& message)
        : HandlerError("invalid_request", message) {}
};

class RevisionConflict : public platform::http::HandlerError<409> {
  public:
    explicit RevisionConflict(const std::string& message) : HandlerError("conflict", message) {}
};

class DraftForbidden : public platform::http::HandlerError<403> {
  public:
    explicit DraftForbidden(const std::string& message) : HandlerError("forbidden", message) {}
};

class RevisionNotFound : public platform::http::HandlerError<404> {
  public:
    explicit RevisionNotFound(const std::string& message) : HandlerError("not_found", message) {}
};
}  // namespace configs
