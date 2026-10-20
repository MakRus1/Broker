#pragma once
#include <deps.hpp>
#include <userver/components/component_base.hpp>
#include <userver/utils/periodic_task.hpp>

namespace shipments {
class TransportationDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportation-dispatcher";
    TransportationDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~TransportationDispatcher() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void Step();
    Deps deps_;
    userver::clients::http::Client& http_;
    std::string token_url_, client_id_, client_secret_;
    userver::utils::PeriodicTask task_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::TransportationDispatcher> = true;
