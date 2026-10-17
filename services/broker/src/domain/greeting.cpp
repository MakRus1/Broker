#include <domain/greeting.hpp>

#include <stdexcept>

namespace broker::domain {

std::string SayHelloTo(std::string_view name, UserType type) {
    if (name.empty()) {
        name = "unknown user";
    }

    switch (type) {
    case UserType::kFirstTime:
        return "Hello, " + std::string{name} + "!\n";
    case UserType::kKnown:
        return "Hi again, " + std::string{name} + "!\n";
    }

    throw std::logic_error("Unknown user type");
}

}  // namespace broker::domain
