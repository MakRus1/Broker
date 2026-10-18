#pragma once

#include <stdexcept>

namespace configs {
class InvalidRequest : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionConflict : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class DraftForbidden : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionNotFound : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
}  // namespace configs
