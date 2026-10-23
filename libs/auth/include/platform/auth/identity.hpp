#pragma once
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
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

inline const std::string& GetAccessToken(const userver::server::request::RequestContext& context) {
    GetIdentity(context);
    return context.GetData<std::string>("platform-auth-access-token");
}

inline bool HasPermission(
    const userver::server::request::RequestContext& context, std::string_view permission) {
    return GetIdentity(context).Permissions.contains(std::string(permission));
}

inline void CheckPermission(
    const userver::server::request::RequestContext& context, std::string_view permission) {
    if (!HasPermission(context, permission))
        throw Forbidden{};
}
}  // namespace platform::auth
