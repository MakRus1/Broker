#pragma once
#include <set>
#include <string>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/request/request_context.hpp>

namespace platform::auth {
struct Identity {
    std::string Issuer;
    std::string Subject;
    std::set<std::string> Permissions;
    std::string DisplayName;

    std::string ActorId() const {
        return Issuer + "#" + Subject;
    }
};

inline const Identity& GetIdentity(const userver::server::request::RequestContext& context) {
    const auto* identity = context.GetDataOptional<Identity>("platform-auth");
    if (!identity)
        throw userver::server::handlers::Unauthorized{};
    return *identity;
}
}  // namespace platform::auth
