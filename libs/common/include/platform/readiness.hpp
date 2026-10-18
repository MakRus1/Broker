#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/state.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/middlewares/http_middleware_base.hpp>

namespace platform {

struct DependencyStatus {
    bool Ready;
    std::string Reason;
};

class Readiness final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "platform-readiness";
    using Check = std::function<DependencyStatus()>;

    struct Entry {
        std::string Name;
        Check Callback;
    };

    // Владелец зависимости хранит регистрацию; после его удаления проверка отключается.
    using Registration = std::shared_ptr<const Entry>;

    Readiness(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : ComponentBase(config, context), state_(context) {}

    Registration Register(std::string name, Check callback) {
        const std::lock_guard lock(mutex_);
        for (const auto& weak : checks_) {
            if (const auto entry = weak.lock(); entry && entry->Name == name)
                throw std::invalid_argument("Duplicate critical dependency");
        }
        auto entry = std::make_shared<const Entry>(Entry{std::move(name), std::move(callback)});
        checks_.push_back(entry);
        return entry;
    }

    DependencyStatus GetStatus() const {
        if (state_.GetServiceLifetimeStage() != userver::components::ServiceLifetimeStage::kRunning)
            return {false, "service is not running"};
        if (state_.IsAnyComponentInFatalState())
            return {false, "component is unavailable"};
        std::vector<Registration> checks;
        {
            const std::lock_guard lock(mutex_);
            for (const auto& weak : checks_)
                if (auto check = weak.lock())
                    checks.push_back(std::move(check));
        }
        for (const auto& entry : checks) {
            try {
                const auto status = entry->Callback();
                if (!status.Ready)
                    return {false, entry->Name + ": " + status.Reason};
            } catch (const std::exception&) {
                return {false, entry->Name + ": check failed"};
            }
        }
        return {true, {}};
    }

  private:
    userver::components::State state_;
    mutable std::mutex mutex_;
    std::vector<std::weak_ptr<const Entry>> checks_;
};

class HttpReadiness final : public userver::server::middlewares::HttpMiddlewareBase {
  public:
    HttpReadiness(const Readiness& readiness, bool exempt)
        : readiness_(readiness), exempt_(exempt) {}

  private:
    void HandleRequest(userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override {
        if (!exempt_ && !readiness_.GetStatus().Ready) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
            request.GetHttpResponse().SetData("dependency unavailable\n");
            return;
        }
        Next(request, context);
    }

    const Readiness& readiness_;
    const bool exempt_;
};

class HttpReadinessFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "platform-http-readiness";

    HttpReadinessFactory(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : HttpMiddlewareFactoryBase(config, context),
          readiness_(context.FindComponent<Readiness>()) {}

  private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase& handler,
        userver::yaml_config::YamlConfig) const override {
        // Служебный listener и выключаемый testsuite-control работают при закрытом gate.
        return std::make_unique<HttpReadiness>(
            readiness_, handler.IsMonitor() || handler.HandlerName() == "tests-control");
    }

    const Readiness& readiness_;
};

}  // namespace platform
