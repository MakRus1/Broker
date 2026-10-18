#pragma once
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <platform/auth/component.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/server/http/http_response_cookie.hpp>

namespace admin_bff {
class CapacityExceeded final : public std::runtime_error {
  public:
    CapacityExceeded() : std::runtime_error("Session capacity exceeded") {}
};

using Clock = std::chrono::steady_clock;

struct Session {
    userver::engine::Mutex Mutex;
    std::string Access, Refresh, Csrf;
    platform::auth::Principal User;
    Clock::time_point TokenExpires, Expires;
};

struct Pending {
    std::string Verifier, Nonce;
    Clock::time_point Expires;
};

class Sessions final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "admin-sessions";
    Sessions(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::pair<std::string, std::string> Begin();
    std::string Complete(const std::string& state, const std::string& code);
    std::shared_ptr<Session> Get(const std::string& id);
    void EnsureFresh(Session& session);
    void Logout(const std::string& id);
    userver::server::http::Cookie Cookie(
        const std::string& name, const std::string& value, int seconds) const;
    std::string LogoutUrl() const;
    std::string CookieName(const std::string& name) const;
    const std::string Origin;

  private:
    userver::formats::json::Value Exchange(const std::string& form) const;
    void Tokens(Session& session, const userver::formats::json::Value& tokens);
    void Cleanup();
    const platform::auth::Component& auth_;
    userver::clients::http::Client& http_;
    const std::string issuer_, token_url_, revoke_url_, client_;
    const bool secure_;
    std::mutex mutex_;
    std::map<std::string, Pending> pending_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
};
}  // namespace admin_bff
