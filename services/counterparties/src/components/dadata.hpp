#pragma once
#include <openapi/models.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <vector>

namespace counterparties {
class DaData final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "dadata";
    DaData(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::RegistryParty> Lookup(const std::string& tax_id) const;

  private:
    userver::clients::http::Client& http_;
    const std::string endpoint_;
    const std::string api_key_;
    const std::chrono::milliseconds timeout_;
};
}  // namespace counterparties

template <> inline constexpr bool userver::components::kHasValidate<counterparties::DaData> = true;
