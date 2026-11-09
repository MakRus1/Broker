#pragma once
#include <optional>
#include <string>

namespace organizations {
struct User {
    std::string Subject;
    std::string Username;
    std::string DisplayName;
    std::string Email;
    std::optional<std::string> LastName;
    std::optional<std::string> FirstName;
    std::optional<std::string> MiddleName;
};

}  // namespace organizations
