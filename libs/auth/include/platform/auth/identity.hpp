#pragma once
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/request/request_context.hpp>

namespace platform::auth {
class Forbidden final : public std::runtime_error {
  public:
    Forbidden() : std::runtime_error("Permission denied") {}
};

struct Identity {
    std::string Issuer;
    std::string Subject;
    std::set<std::string> Permissions;
    std::string DisplayName;
    std::optional<std::string> Organization;
    std::string ClientId;

    std::string ActorId() const {
        return Issuer + "#" + Subject;
    }
};

inline const std::string& GetOrganization(const Identity& identity) {
    if (!identity.Organization.has_value())
        throw Forbidden{};
    return identity.Organization.value();
}

inline const Identity& GetIdentity(const userver::server::request::RequestContext& context) {
    const auto* identity = context.GetDataOptional<Identity>("platform-auth");
    if (!identity)
        throw userver::server::handlers::Unauthorized{};
    return *identity;
}
}  // namespace platform::auth
