#pragma once
#include <set>
#include <string>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/request/request_context.hpp>

namespace platform::auth {
struct Principal {
    std::string Issuer;
    std::string Subject;
    std::set<std::string> Permissions;
    std::string ActorId() const {
        return Issuer + "#" + Subject;
    }
};
inline const Principal& GetPrincipal(const userver::server::request::RequestContext& context) {
    const auto* principal = context.GetDataOptional<Principal>("platform-auth");
    if (!principal)
        throw userver::server::handlers::Unauthorized{};
    return *principal;
}
}  // namespace platform::auth
