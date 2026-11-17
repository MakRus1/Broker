#pragma once
#include <chrono>
#include <platform/auth/component.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/http/http_response_cookie.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/periodic_task.hpp>

namespace platform::browser {
class CapacityExceeded final : public std::runtime_error {
  public:
    CapacityExceeded() : std::runtime_error("Session capacity exceeded") {}
};

using Clock = std::chrono::system_clock;

struct Session {
    std::string Access, Refresh, Csrf;
    platform::auth::Identity User;
    Clock::time_point TokenExpires, Expires;
};

struct SessionSnapshot {
    platform::auth::Identity User;
    std::string Access, Csrf;
};

struct Pending {
    std::string Verifier, Nonce;
};

class Sessions final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "browser-sessions";
    static constexpr auto kLoginTtl = std::chrono::seconds{std::chrono::minutes{5}};
    static constexpr auto kSessionTtl = std::chrono::seconds{std::chrono::hours{8}};
    Sessions(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::pair<std::string, std::string> Begin();
    std::string Complete(const std::string& state, const std::string& code);
    SessionSnapshot GetAuthenticated(const std::string& id);
    void Logout(const std::string& id);
    userver::server::http::Cookie Cookie(
        const std::string& name, const std::string& value, int seconds) const;
    std::string LogoutUrl() const;
    std::string CookieName(const std::string& name) const;
    const std::string Origin;
    const std::string SessionCookie;
    const std::string LoginCookie;

  private:
    userver::formats::json::Value Exchange(const std::string& form) const;
    void Tokens(Session& session, const userver::formats::json::Value& tokens);
    void Refresh(Session& session, const std::string& actor);
    platform::auth::Identity VerifyAccess(const std::string& token) const;
    void StorePending(const std::string& state, const Pending& pending);
    Pending ConsumePending(const std::string& state);
    void StoreSession(const std::string& id, const Session& session);
    std::string RemoveSession(const std::string& id);
    const platform::auth::Component& auth_;
    userver::clients::http::Client& http_;
    const std::string issuer_, token_url_, revoke_url_, client_;
    const bool secure_;
    userver::storages::postgres::ClusterPtr cluster_;
    userver::utils::PeriodicTask cleanup_task_;
};
}  // namespace platform::browser
