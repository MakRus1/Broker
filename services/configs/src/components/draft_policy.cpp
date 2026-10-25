#include <components/draft_policy.hpp>
#include <userver/components/component_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace configs {
DraftPolicy::DraftPolicy(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      allow_self_approval_(config["allow-self-approval"].As<bool>(false)) {}

userver::yaml_config::Schema DraftPolicy::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Правила подтверждения конфигов для окружения
additionalProperties: false
properties:
    allow-self-approval:
        type: boolean
        description: Разрешить автору с configs.approve подтверждать свой драфт
)");
}
}  // namespace configs
