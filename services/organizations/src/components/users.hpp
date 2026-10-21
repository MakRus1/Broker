#pragma once
#include <platform/auth/service_token.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>

namespace organizations {
struct User {
    std::string Subject;
    std::string Username;
    std::string DisplayName;
    std::string Email;
};

class Users final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "keycloak-users";
    Users(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    User Find(const std::string& username) const;
    User Create(const std::string& request_id, const std::string& username,
        const std::string& email, const std::string& display_name,
        const std::string& temporary_password) const;

  private:
    userver::formats::json::Value Lookup(const std::string& username) const;
    userver::clients::http::Client& http_;
    const platform::auth::ServiceTokenProvider& tokens_;
    const std::string base_url_;
    const std::chrono::milliseconds timeout_;
};
}  // namespace organizations

template <> inline constexpr bool userver::components::kHasValidate<organizations::Users> = true;
