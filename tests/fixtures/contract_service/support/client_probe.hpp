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
                const auto response = deps_.broker.helloPost({.name = http.GetArg("name")});
                result["value"] = std::get<clients::broker::helloPost::Response200>(response).body;
                return userver::formats::json::ToString(result.ExtractValue());
            }
            const auto response = deps_.broker.getFooBar({.name = http.GetArg("name")});
            std::visit(
                [&](const auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, clients::broker::getFooBar::Response200>) {
                        result["status"] = 200;
                        result["value"] = value.greeting;
                    } else {
                        result["status"] = 404;
                        result["value"] = value.message;
                    }
                },
                response);
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

}  // namespace contract_service
