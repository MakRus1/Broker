#pragma once

#include <deps.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace contract_service {

// Только тестовый сервис: вызов сгенерированного клиента через общий Deps.
class ClientProbe final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "client-probe";

    ClientProbe(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : HttpHandlerBase(config, context), deps_(ResolveDeps(context)) {}

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& http,
        userver::server::request::RequestContext&) const override {
        userver::formats::json::ValueBuilder result;
        try {
            if (http.GetArg("method") == "post") {
                const auto response = deps_.broker.HelloCreate({.name = http.GetArg("name")});
                result["value"] = response.body;
                return userver::formats::json::ToString(result.ExtractValue());
            }
            const auto response = deps_.broker.FooBar({.name = http.GetArg("name")});
            result["status"] = 200;
            result["value"] = response.greeting;
        } catch (const clients::broker::FooBar::Error404& error) {
            result["status"] = error.GetStatus();
            result["value"] = error.response.message;
        } catch (const platform::http::TimeoutError&) {
            result["error"] = "timeout";
        } catch (const platform::http::ContractError&) {
            result["error"] = "contract";
        } catch (const platform::http::TransportError&) {
            result["error"] = "transport";
        }
        return userver::formats::json::ToString(result.ExtractValue());
    }

  private:
    Deps deps_;
};

// Только тестовый сервис: служебные ручки работают и при закрытом gate.
class ReadinessProbe final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "handler-ping";

    ReadinessProbe(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : HttpHandlerBase(config, context),
          readiness_(context.FindComponent<platform::Readiness>()), state_(context) {}

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const override {
        if (HandlerName() == "handler-ping") {
            if (state_.GetServiceLifetimeStage() !=
                userver::components::ServiceLifetimeStage::kRunning)
                request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
            return "alive\n";
        }
        const auto status = readiness_.GetStatus();
        if (!status.Ready)
            request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return status.Ready ? "ready\n" : status.Reason + "\n";
    }

  private:
    const platform::Readiness& readiness_;
    userver::components::State state_;
};

}  // namespace contract_service
