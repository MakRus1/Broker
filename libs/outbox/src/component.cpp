#include <platform/outbox/component.hpp>
#include <platform_outbox_sql/sql_queries.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform::outbox {
namespace sql = platform_outbox_sql::sql;

void Enqueue(pg::Transaction& transaction, std::string_view topic, std::string_view key,
    const userver::formats::json::Value& payload) {
    if (topic.empty() || key.empty())
        throw std::invalid_argument{"Тип и ключ сообщения outbox должны быть заполнены"};
    transaction.Execute(sql::kEnqueue, std::string{topic}, std::string{key}, payload);
}

void EnqueueLatest(pg::Transaction& transaction, std::string_view topic, std::string_view key,
    const userver::formats::json::Value& payload) {
    if (topic.empty() || key.empty())
        throw std::invalid_argument{"Тип и ключ сообщения outbox должны быть заполнены"};
    transaction.Execute(sql::kEnqueueLatest, std::string{topic}, std::string{key}, payload);
}

Component::Component(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context
                   .FindComponent<userver::components::Postgres>(
                       config["postgres-component"].As<std::string>())
                   .GetCluster()),
      settings_{.BatchSize = config["batch-size"].As<std::size_t>(50),
          .BusyPause = std::chrono::milliseconds{config["busy-pause-ms"].As<int>(100)},
          .IdlePause = std::chrono::milliseconds{config["idle-pause-ms"].As<int>(1000)}},
      lease_seconds_(config["lease-seconds"].As<int>(30)),
      retry_seconds_(config["retry-seconds"].As<int>(2)),
      max_retry_seconds_(config["max-retry-seconds"].As<int>(60)) {
    if (lease_seconds_ <= 0 || retry_seconds_ <= 0 || max_retry_seconds_ < retry_seconds_ ||
        settings_.BatchSize == 0 || settings_.BusyPause.count() <= 0 ||
        settings_.IdlePause < settings_.BusyPause)
        throw std::invalid_argument{"Некорректные настройки outbox"};
    // Только проверка. Схему создаёт релизная миграция, а не runtime компонента.
    cluster_->Execute(pg::ClusterHostType::kMaster, sql::kCheckSchema);
}

Component::~Component() {
    worker_.Stop();
}

void Component::Register(std::string topic, Handler handler) {
    if (started_ || topic.empty() || !handlers_.emplace(topic, std::move(handler)).second)
        throw std::logic_error{"Обработчик outbox должен быть зарегистрирован один раз до запуска"};
    topics_.push_back(std::move(topic));
}

void Component::OnAllComponentsLoaded() {
    started_ = true;
    if (topics_.empty())
        return;
    worker_.Start(
        "outbox-delivery", settings_, [this] { return Claim(); },
        [this](const Message& message) { return handlers_.at(message.Topic)(message); },
        [this](const Message& message, const Completion& complete) { Complete(message, complete); },
        [this](const Message& message) { Retry(message); });
}

void Component::OnAllComponentsAreStopping() {
    worker_.Stop();
}

std::optional<Message> Component::Claim() {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kClaim, topics_, lease_seconds_);
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<Message>(pg::kRowTag);
}

void Component::Complete(const Message& message, const Completion& complete) {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked =
        transaction.Execute(sql::kLockDelivery, message.Id, message.Lease, message.Generation);
    if (!locked.IsEmpty()) {
        if (complete)
            complete(transaction);
        transaction.Execute(sql::kComplete, message.Id, message.Lease, message.Generation);
    }
    transaction.Commit();
}

void Component::Retry(const Message& message) {
    cluster_->Execute(pg::ClusterHostType::kMaster, sql::kRetry, message.Id, message.Lease,
        retry_seconds_, max_retry_seconds_);
}

userver::yaml_config::Schema Component::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Доставка сообщений стандартной PostgreSQL outbox
additionalProperties: false
required: [postgres-component]
properties:
    postgres-component:
        type: string
        description: Компонент кластера PostgreSQL со стандартной таблицей outbox
    batch-size:
        type: integer
        minimum: 1
        description: Максимум сообщений в одной пачке
        defaultDescription: 50
    busy-pause-ms:
        type: integer
        minimum: 1
        description: Пауза после полной пачки в миллисекундах
        defaultDescription: 100
    idle-pause-ms:
        type: integer
        minimum: 1
        description: Пауза пустой очереди и ошибок хранилища в миллисекундах
        defaultDescription: 1000
    lease-seconds:
        type: integer
        minimum: 1
        description: Срок захвата сообщения в секундах
        defaultDescription: 30
    retry-seconds:
        type: integer
        minimum: 1
        description: Базовая задержка повтора, умножаемая на число попыток
        defaultDescription: 2
    max-retry-seconds:
        type: integer
        minimum: 1
        description: Максимальная задержка повтора в секундах
        defaultDescription: 60
)");
}
}  // namespace platform::outbox
