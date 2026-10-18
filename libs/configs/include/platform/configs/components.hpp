#pragma once

#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/raw_component_base.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updates_sink/find.hpp>
#include <userver/dynamic_config/value.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform::configs {

struct Policy {
    bool AllowBuiltin;
    bool AllowCache;
    std::chrono::seconds MaxAge;

    DependencyStatus Evaluate(
        const Metadata& metadata, std::chrono::system_clock::time_point now) const {
        if (metadata.Source == Origin::Builtin)
            return {AllowBuiltin, AllowBuiltin ? "" : "builtin value is forbidden"};
        if (metadata.Source == Origin::Incompatible)
            return {false, "incompatible cache schema"};
        if (metadata.Source == Origin::Cache && !AllowCache)
            return {false, "disk cache is forbidden"};
        if (!metadata.LastSuccessfulUpdate || now < *metadata.LastSuccessfulUpdate ||
            now - *metadata.LastSuccessfulUpdate > MaxAge)
            return {false, "config confirmation expired"};
        return {true, {}};
    }
};

// Полные обновления дают подтверждение наличия каждого ключа, даже без изменения значения.
// Метаданные сохраняются в том же snapshot и файловом кеше, что и сами значения.
template <typename Contract>
class DeliverySink final : public userver::components::DynamicConfigUpdatesSinkBase {
  public:
    static constexpr std::string_view kName = "platform-config-delivery";
    using Base = userver::components::DynamicConfigUpdatesSinkBase;
    using Base::SetConfig;

    DeliverySink(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : Base(config, context), sink_(userver::dynamic_config::FindUpdatesSink(config, context)) {
        auto& storage = context.FindComponent<userver::components::DynamicConfig>();
        // Пригодный кеш уже загружен storage. Без него запускаемся с defaults, сохраняя
        // возможность отвечать на ping до восстановления сервера конфигов.
        bool bootstrap =
            static_cast<userver::components::RawComponentBase&>(storage).GetComponentHealth() !=
            userver::components::ComponentHealth::kOk;
        if (!bootstrap) {
            const auto snapshot = storage.GetSource().GetSnapshot();
            bootstrap = !snapshot[kDeliveryMetadata].HasMember("process");
        }
        if (bootstrap)
            sink_.SetConfig(kName, storage.GetDefaultDocsMap());
    }

    void SetConfig(std::string_view updater, userver::dynamic_config::DocsMap&& docs) override {
        Contract::Validate(docs);
        userver::formats::json::ValueBuilder metadata;
        metadata["process"] = ProcessToken();
        metadata["received_at_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            userver::utils::datetime::Now().time_since_epoch())
                                         .count();
        metadata["schemas"] =
            userver::formats::json::ValueBuilder(userver::formats::json::Type::kObject);
        for (const auto& [name, hash] : Contract::Schemas())
            if (docs.Has(name))
                metadata["schemas"][name] = hash;
        docs.Set("BROKER_CONFIG_DELIVERY_META", metadata.ExtractValue());
        sink_.SetConfig(updater, std::move(docs));
    }

    void NotifyLoadingFailed(std::string_view updater, std::string_view error) override {
        sink_.NotifyLoadingFailed(updater, error);
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<Base>(R"(
type: object
description: Подтверждение доставки конфигов и происхождение снимка
additionalProperties: false
properties:
    updates-sink:
        type: string
        description: Хранилище dynamic-config
)");
    }

  private:
    Base& sink_;
};

class ConfigReadiness final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "platform-config-readiness";

    ConfigReadiness(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : ComponentBase(config, context) {
        auto& readiness = context.FindComponent<platform::Readiness>();
        const Source source{
            context.FindComponent<userver::components::DynamicConfig>().GetSource()};
        for (const auto& rule : config["rules"]) {
            const auto name = rule["name"].As<std::string>();
            const auto hash = rule["schema-hash"].As<std::string>();
            const Policy policy{rule["allow-builtin"].As<bool>(), rule["allow-cache"].As<bool>(),
                std::chrono::seconds{rule["max-age-seconds"].As<int>()}};
            registrations_.push_back(
                readiness.Register("config:" + name, [source, name, hash, policy] {
                    return policy.Evaluate(source.GetSnapshot().GetMetadata(name, hash),
                        userver::utils::datetime::Now());
                }));
        }
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<ComponentBase>(R"(
type: object
description: Обязательные конфиги сервиса
additionalProperties: false
properties:
    rules:
        type: array
        description: Политики готовности по ключам
        items:
            type: object
            description: Политика одного обязательного ключа
            additionalProperties: false
            properties:
                name:
                    type: string
                    description: Глобальное имя ключа
                schema-hash:
                    type: string
                    description: Версия схемы потребителя
                allow-builtin:
                    type: boolean
                    description: Разрешено встроенное значение
                allow-cache:
                    type: boolean
                    description: Разрешен дисковый кеш
                max-age-seconds:
                    type: integer
                    minimum: 1
                    description: Срок с последнего успешного получения
)");
    }

  private:
    std::vector<platform::Readiness::Registration> registrations_;
};

}  // namespace platform::configs
