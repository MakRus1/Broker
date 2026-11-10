#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <platform/browser/session.hpp>
#include <userver/components/component_base.hpp>

namespace admin_bff {
class AdminSessionComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "admin-session";
    AdminSessionComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::AdminSession Get(const platform::browser::BrowserSession&) const;

  private:
    clients::organizations::Client organizations_;
};
}  // namespace admin_bff

template <>
inline constexpr bool userver::components::kHasValidate<admin_bff::AdminSessionComponent> = true;
