#pragma once
#include <memory>
#include <platform/auth/principal.hpp>
#include <stdexcept>
#include <userver/server/middlewares/http_middleware_base.hpp>

namespace platform::auth {
class Unavailable final : public std::runtime_error {
  public:
    Unavailable() : std::runtime_error("Authentication keys unavailable") {}
};
class Forbidden final : public std::runtime_error {
  public:
    Forbidden() : std::runtime_error("Permission denied") {}
};
class Component final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "platform-auth";
    Component(const userver::components::ComponentConfig&,
              const userver::components::ComponentContext&);
    ~Component() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();
    bool IsProtected(std::string_view path) const;
    Principal Authorize(const std::string& path, const std::string& authorization) const;

  private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase>
    Create(const userver::server::handlers::HttpHandlerBase&,
           userver::yaml_config::YamlConfig) const override;
    struct State;
    std::unique_ptr<State> state_;
};
}  // namespace platform::auth

template <>
inline constexpr bool userver::components::kHasValidate<platform::auth::Component> = true;
