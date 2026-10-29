#include <algorithm>
#include <platform/outbox/component.hpp>
#include <platform_outbox_sql/sql_queries.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/statistics_storage.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/testsuite/testsuite_support.hpp>
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
      max_retry_seconds_(config["max-retry-seconds"].As<int>(60)),
      max_concurrency_(config["max-concurrency"].As<std::size_t>(8)),
      default_topic_concurrency_(config["default-topic-concurrency"].As<std::size_t>(2)),
      statistics_period_(config["statistics-period-ms"].As<int>(30000)),
      statistics_registration_(
          context.FindComponent<userver::components::TestsuiteSupport>().GetPeriodicTaskControl(),
          "outbox-queue-statistics", statistics_task_) {
    if (statistics_period_.count() <= 0)
        throw std::invalid_argument{"Период метрик outbox должен быть положительным"};
    userver::utils::statistics::RegisterWriterScope(
        context, "outbox", [this](auto& writer) { WriteStatistics(writer); });
    for (const auto& entry :
        config["topic-concurrency"].As<std::map<std::string, std::size_t>>({})) {
        if (entry.second == 0)
            throw std::invalid_argument{"Лимит типа outbox должен быть положительным"};
        topic_concurrency_.emplace(entry);
    }
    if (max_concurrency_ == 0 || default_topic_concurrency_ == 0)
        throw std::invalid_argument{"Лимиты параллельности outbox должны быть положительными"};
    if (lease_seconds_ <= 0 || retry_seconds_ <= 0 || max_retry_seconds_ < retry_seconds_ ||
        settings_.BatchSize == 0 || settings_.BusyPause.count() <= 0 ||
        settings_.IdlePause < settings_.BusyPause)
        throw std::invalid_argument{"Некорректные настройки outbox"};
    // Только проверка. Схему создаёт релизная миграция, а не runtime компонента.
    cluster_->Execute(pg::ClusterHostType::kMaster, sql::kCheckSchema);
}

Component::~Component() {
    Stop();
}

void Component::Register(std::string topic, Handler handler) {
    if (started_ || topic.empty() || !handlers_.emplace(topic, std::move(handler)).second)
        throw std::logic_error{"Обработчик outbox должен быть зарегистрирован один раз до запуска"};
    {
        const std::lock_guard lock(statistics_mutex_);
        statistics_.emplace(topic, std::make_unique<TopicStatistics>());
    }
    topics_.push_back(std::move(topic));
}

void Component::OnAllComponentsLoaded() {
    started_ = true;
    std::size_t total = 0;
    for (const auto& [topic, limit] : topic_concurrency_) {
        if (!handlers_.contains(topic))
            throw std::invalid_argument{
                "Лимит задан для незарегистрированного типа outbox: " + topic};
    }
    for (const auto& topic : topics_) {
        const auto found = topic_concurrency_.find(topic);
        const auto limit =
            found == topic_concurrency_.end() ? default_topic_concurrency_ : found->second;
        if (limit > max_concurrency_ - total)
            throw std::invalid_argument{"Сумма лимитов типов outbox превышает max-concurrency"};
        total += limit;
    }
    workers_.reserve(total);
    for (const auto& topic : topics_) {
        const auto found = topic_concurrency_.find(topic);
        const auto limit =
            found == topic_concurrency_.end() ? default_topic_concurrency_ : found->second;
        for (std::size_t slot = 0; slot < limit; ++slot) {
            auto worker = std::make_unique<Worker>();
            auto& stats = *statistics_.at(topic);
            stats.Concurrency = limit;
            worker->Start("outbox/" + topic + "/" + std::to_string(slot), settings_,
                [this, topic, &stats] { return ProcessOne(topic, stats); });
            workers_.push_back(std::move(worker));
        }
    }
    statistics_task_.Start(
        "outbox-queue-statistics", statistics_period_, [this] { RefreshQueueStatistics(); });
    statistics_task_.ForceStepAsync();
}

void Component::Stop() noexcept {
    statistics_task_.Stop();
    for (auto& worker : workers_)
        worker->Stop();
}

void Component::OnAllComponentsAreStopping() {
    Stop();
}

std::optional<Message> Component::Claim(const std::string& topic) {
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kClaim, topic, lease_seconds_);
    if (rows.IsEmpty())
        return std::nullopt;
    return rows.AsSingleRow<Message>(pg::kRowTag);
}

bool Component::Complete(const Message& message, const Completion& complete) {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked =
        transaction.Execute(sql::kLockDelivery, message.Id, message.Lease, message.Generation);
    if (!locked.IsEmpty()) {
        if (complete)
            complete(transaction);
        transaction.Execute(sql::kComplete, message.Id, message.Lease, message.Generation);
    }
    transaction.Commit();
    return !locked.IsEmpty();
}

void Component::Retry(const Message& message) {
    const auto result = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kRetry, message.Id,
        message.Lease, retry_seconds_, max_retry_seconds_);
    if (result.RowsAffected() != 0)
        ++statistics_.at(message.Topic)->Retried;
}

namespace {
double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
}  // namespace

bool Component::ProcessOne(const std::string& topic, TopicStatistics& stats) {
    struct ActiveDelivery {
        TopicStatistics& Stats;
        std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();

        explicit ActiveDelivery(TopicStatistics& stats) : Stats(stats) {
            ++Stats.Active;
        }

        ~ActiveDelivery() {
            --Stats.Active;
            ++Stats.Finished;
            Stats.DurationUs += std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - Started)
                                    .count();
        }
    };

    std::optional<ActiveDelivery> active;
    auto claim = [&] {
        try {
            auto message = Claim(topic);
            if (message.has_value()) {
                ++stats.Attempts;
                active.emplace(stats);
            }
            return message;
        } catch (const std::exception&) {
            if (!userver::engine::current_task::ShouldCancel())
                ++stats.StorageErrors;
            throw;
        }
    };
    auto send = [&](const Message& message) {
        try {
            return handlers_.at(topic)(message);
        } catch (const std::exception&) {
            if (!userver::engine::current_task::ShouldCancel())
                ++stats.Failed;
            throw;
        }
    };
    auto complete = [&](const Message& message, const Completion& completion) {
        try {
            if (Complete(message, completion))
                ++stats.Delivered;
            else
                ++stats.Superseded;
        } catch (const std::exception&) {
            if (!userver::engine::current_task::ShouldCancel()) {
                ++stats.Failed;
                ++stats.StorageErrors;
            }
            throw;
        }
    };
    auto retry = [&](const Message& message) {
        try {
            Retry(message);
        } catch (const std::exception&) {
            if (!userver::engine::current_task::ShouldCancel())
                ++stats.StorageErrors;
            throw;
        }
    };
    try {
        return DeliverOne(claim, send, complete, retry);
    } catch (const std::exception&) {
        if (active.has_value() && userver::engine::current_task::ShouldCancel())
            ++stats.Cancelled;
        throw;
    }
}

void Component::RefreshQueueStatistics() {
    struct QueueRow {
        std::string Topic;
        std::int64_t Size;
        double OldestCreatedAt;
    };

    try {
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kQueueStatistics, topics_)
                .AsContainer<std::vector<QueueRow>>(pg::kRowTag);
        const std::lock_guard lock(statistics_mutex_);
        for (auto& [topic, stats] : statistics_) {
            stats->QueueSize = 0;
            stats->OldestCreatedAt = 0;
        }
        for (const auto& row : rows) {
            auto& stats = *statistics_.at(row.Topic);
            stats.QueueSize = row.Size;
            stats.OldestCreatedAt = row.OldestCreatedAt;
        }
        snapshot_timestamp_ = NowSeconds();
        snapshot_success_ = true;
    } catch (const std::exception&) {
        if (!userver::engine::current_task::ShouldCancel()) {
            snapshot_success_ = false;
            ++snapshot_errors_;
        }
        throw;
    }
}

void Component::WriteStatistics(userver::utils::statistics::Writer& writer) {
    using userver::utils::statistics::Rate;
    const auto now = NowSeconds();
    writer["queue_snapshot_success"] = snapshot_success_.load() ? 1 : 0;
    writer["queue_snapshot_timestamp_seconds"] = snapshot_timestamp_.load();
    writer["queue_snapshot_errors_total"] = Rate{snapshot_errors_.load()};
    const std::lock_guard lock(statistics_mutex_);
    for (const auto& [topic, stats] : statistics_) {
        const auto labels = {userver::utils::statistics::LabelView{"outbox_topic", topic}};
        writer["active"].ValueWithLabels(stats->Active.load(), labels);
        writer["concurrency"].ValueWithLabels(stats->Concurrency.load(), labels);
        writer["queue_size"].ValueWithLabels(stats->QueueSize.load(), labels);
        const auto oldest = stats->OldestCreatedAt.load();
        writer["oldest_message_age_seconds"].ValueWithLabels(
            oldest == 0 ? 0.0 : std::max(0.0, now - oldest), labels);
        writer["attempts_total"].ValueWithLabels(Rate{stats->Attempts.load()}, labels);
        writer["delivered_total"].ValueWithLabels(Rate{stats->Delivered.load()}, labels);
        writer["failed_total"].ValueWithLabels(Rate{stats->Failed.load()}, labels);
        writer["retries_total"].ValueWithLabels(Rate{stats->Retried.load()}, labels);
        writer["superseded_total"].ValueWithLabels(Rate{stats->Superseded.load()}, labels);
        writer["cancelled_total"].ValueWithLabels(Rate{stats->Cancelled.load()}, labels);
        writer["storage_errors_total"].ValueWithLabels(Rate{stats->StorageErrors.load()}, labels);
        writer["finished_total"].ValueWithLabels(Rate{stats->Finished.load()}, labels);
        writer["duration_microseconds_total"].ValueWithLabels(
            Rate{stats->DurationUs.load()}, labels);
    }
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
    max-concurrency:
        type: integer
        minimum: 1
        description: Верхняя граница суммы выделенных слотов всех типов на одном поде
        defaultDescription: 8
    default-topic-concurrency:
        type: integer
        minimum: 1
        description: Число независимых доставок каждого типа по умолчанию
        defaultDescription: 2
    topic-concurrency:
        type: object
        properties: {}
        description: Число выделенных слотов доставки по имени типа сообщения
        additionalProperties:
            type: integer
            minimum: 1
            description: Максимум одновременных доставок этого типа
    statistics-period-ms:
        type: integer
        minimum: 1
        description: Период фонового снимка размера и возраста очередей в миллисекундах
        defaultDescription: 30000
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
