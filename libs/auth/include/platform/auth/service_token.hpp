#pragma once
#include <chrono>
#include <string>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>

namespace platform::auth {
// Отдельный клиент OAuth: секреты, формат запроса и транспортная политика не нужны бизнес-коду.
class ServiceTokenProvider final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "service-token-provider";
    ServiceTokenProvider(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string GetAccessToken() const;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    userver::clients::http::Client& http_;
    std::string endpoint_;
    std::string client_id_;
    std::string client_secret_;
    std::chrono::milliseconds timeout_;
    int retries_;
};
}  // namespace platform::auth

template <>
inline constexpr bool userver::components::kHasValidate<platform::auth::ServiceTokenProvider> =
    true;
