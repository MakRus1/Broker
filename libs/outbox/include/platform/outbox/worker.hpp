#pragma once
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <userver/engine/task/cancel.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/periodic_task.hpp>

namespace platform::outbox {

struct Settings {
    std::size_t BatchSize = 50;
    std::chrono::milliseconds BusyPause{100};
    std::chrono::milliseconds IdlePause{1000};
};

// Claim возвращает optional сообщения после завершения атомарного захвата в хранилище.
// Send не держит транзакцию. Complete подтверждает именно захваченный lease, Retry
// освобождает его и назначает следующую попытку. Ошибка подтверждения допускает повтор Send.
template <typename Claim, typename Send, typename Complete, typename Retry>
bool DeliverOne(Claim& claim, Send& send, Complete& complete, Retry& retry) {
    auto delivery = std::invoke(claim);
    if (!delivery.has_value())
        return false;
    try {
        if constexpr (std::is_void_v<std::invoke_result_t<Send&, decltype(delivery.value())>>) {
            std::invoke(send, delivery.value());
            std::invoke(complete, delivery.value());
        } else {
            auto result = std::invoke(send, delivery.value());
            std::invoke(complete, delivery.value(), std::move(result));
        }
    } catch (const std::exception& error) {
        if (userver::engine::current_task::ShouldCancel())
            throw;
        LOG_WARNING() << "Outbox delivery deferred: " << error.what();
        std::invoke(retry, delivery.value());
    }
    return true;
}

template <typename Step> std::size_t DeliverBatch(std::size_t limit, Step&& step) {
    std::size_t processed = 0;
    for (; processed < limit; ++processed) {
        if (userver::engine::current_task::ShouldCancel() || !std::invoke(step))
            break;
    }
    return processed;
}

// Один worker обрабатывает очередь последовательно; параллельные поды координирует Claim.
// Владелец обязан вызвать Stop до разрушения зависимостей, захваченных callbacks.
class Worker final {
  public:
    template <typename Claim, typename Send, typename Complete, typename Retry>
    void Start(std::string name, Settings settings, Claim claim, Send send, Complete complete,
        Retry retry) {
        Start(std::move(name), settings,
            [claim = std::move(claim), send = std::move(send), complete = std::move(complete),
                retry = std::move(retry)]() mutable {
                return DeliverOne(claim, send, complete, retry);
            });
    }

    template <typename Step> void Start(std::string name, Settings settings, Step step) {
        if (settings.BatchSize == 0 || settings.BusyPause.count() <= 0 ||
            settings.IdlePause < settings.BusyPause)
            throw std::invalid_argument{"Некорректные настройки outbox worker"};
        task_.Start(std::move(name), TaskSettings(settings.IdlePause, settings.IdlePause),
            [this, settings, step = std::move(step)]() mutable {
                const auto processed = DeliverBatch(settings.BatchSize, step);
                const auto pause =
                    processed == settings.BatchSize ? settings.BusyPause : settings.IdlePause;
                task_.SetSettings(TaskSettings(pause, settings.IdlePause));
            });
    }

    void Stop() noexcept {
        task_.Stop();
    }

  private:
    static userver::utils::PeriodicTask::Settings TaskSettings(
        std::chrono::milliseconds pause, std::chrono::milliseconds error_pause) {
        userver::utils::PeriodicTask::Settings settings{pause};
        settings.exception_period = error_pause;
        return settings;
    }

    userver::utils::PeriodicTask task_;
};
}  // namespace platform::outbox
