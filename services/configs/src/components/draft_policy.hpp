#pragma once
#include <userver/components/component_base.hpp>

namespace configs {
class DraftPolicy final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "config-draft-policy";
    DraftPolicy(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    bool AllowSelfApproval() const {
        return allow_self_approval_;
    }

  private:
    bool allow_self_approval_;
};
}  // namespace configs

template <> inline constexpr bool userver::components::kHasValidate<configs::DraftPolicy> = true;
