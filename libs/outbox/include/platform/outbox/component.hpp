#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <platform/outbox/worker.hpp>
#include <string>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <vector>

namespace platform::outbox {
namespace pg = userver::storages::postgres;

// Enqueue вызывается в транзакции бизнесового изменения. Key задаёт идемпотентность
// среди ещё не подтверждённых сообщений; получатель также обязан быть идемпотентным.
void Enqueue(pg::Transaction&, std::string_view topic, std::string_view key,
    const userver::formats::json::Value& payload);
// Заменяет недоставленное состояние, инвалидируя старый lease и подтверждение.
void EnqueueLatest(pg::Transaction&, std::string_view topic, std::string_view key,
    const userver::formats::json::Value& payload);

struct Message {
    std::string Id;
    std::string Topic;
    userver::formats::json::Value Payload;
    std::string Lease;
    std::int64_t Generation;
};

class Component final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "platform-outbox";
    Component(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~Component() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

    // Регистрация в конструкторе компонента-владельца; обработка начинается только
    // после загрузки всех компонентов и останавливается до их разрушения.
    template <typename Send> void RegisterHandler(std::string topic, Send send) {
        Register(std::move(topic), [send = std::move(send)](const Message& message) {
            std::invoke(send, message);
            return Completion{};
        });
    }

    template <typename Send, typename Complete>
    void RegisterHandler(std::string topic, Send send, Complete complete) {
        Register(std::move(topic),
            [send = std::move(send), complete = std::move(complete)](const Message& message) {
                auto result = std::invoke(send, message);
                return Completion{
                    [message, result = std::move(result), complete](pg::Transaction& transaction) {
                        std::invoke(complete, transaction, message, result);
                    }};
            });
    }

    void OnAllComponentsLoaded() override;
    void OnAllComponentsAreStopping() override;

  private:
    using Completion = std::function<void(pg::Transaction&)>;
    using Handler = std::function<Completion(const Message&)>;
    void Register(std::string, Handler);
    std::optional<Message> Claim();
    void Complete(const Message&, const Completion&);
    void Retry(const Message&);
    pg::ClusterPtr cluster_;
    Settings settings_;
    int lease_seconds_;
    int retry_seconds_;
    int max_retry_seconds_;
    bool started_ = false;
    std::map<std::string, Handler> handlers_;
    std::vector<std::string> topics_;
    Worker worker_;
};
}  // namespace platform::outbox

template <>
inline constexpr bool userver::components::kHasValidate<platform::outbox::Component> = true;
