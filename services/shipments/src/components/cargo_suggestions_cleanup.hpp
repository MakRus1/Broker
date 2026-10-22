#pragma once
#include <atomic>
#include <deps.hpp>
#include <userver/components/component_base.hpp>
#include <userver/engine/mutex.hpp>
#include <userver/testsuite/periodic_task_control.hpp>
#include <userver/utils/periodic_task.hpp>

namespace shipments {
class CargoSuggestionsCleanup final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "cargo-suggestions-cleanup";
    CargoSuggestionsCleanup(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~CargoSuggestionsCleanup() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void Cleanup();
    Deps deps_;
    std::atomic<bool> pending_{false};
    userver::engine::Mutex mutex_;
    userver::utils::PeriodicTask task_;
    userver::utils::PeriodicTask retry_task_;
    userver::testsuite::PeriodicTaskRegistrationHolder registration_;
    userver::testsuite::PeriodicTaskRegistrationHolder retry_registration_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::CargoSuggestionsCleanup> = true;
