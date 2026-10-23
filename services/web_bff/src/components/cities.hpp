#pragma once
#include <openapi/models.hpp>
#include <stdexcept>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <vector>

namespace web_bff {
class CityLookupError final : public std::runtime_error {
  public:
    CityLookupError(std::string code, std::string message)
        : std::runtime_error(std::move(message)), Code(std::move(code)) {}

    std::string Code;
};

class Cities final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "geography-cities";
    Cities(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::City> Find(const std::string& query) const;

  private:
    userver::clients::http::Client& http_;
    std::string endpoint_;
    std::string api_key_;
    std::chrono::milliseconds timeout_;
};
}  // namespace web_bff

template <> inline constexpr bool userver::components::kHasValidate<web_bff::Cities> = true;
