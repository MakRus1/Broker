#pragma once

#include <components/config_store.hpp>
#include <iomanip>
#include <sstream>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_response.hpp>

namespace configs {

class ConfigHandler final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "config-values";
    ConfigHandler(const userver::components::ComponentConfig& config,
                  const userver::components::ComponentContext& context)
        : HttpHandlerBase(config, context), name_(config.Name()),
          store_(context.FindComponent<ConfigStore>()) {}

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                   userver::server::request::RequestContext&) const override {
        request.GetHttpResponse().SetHeader(std::string_view{"Content-Type"}, "application/json");
        try {
            if (name_ == "config-history")
                return userver::formats::json::ToString(store_.History());
            const auto body = userver::formats::json::FromString(request.RequestBody());
            if (name_ == "config-values") {
                platform::configs::ObjectKeys(body, {"ids", "updated_since"});
                const auto current = store_.Current();
                userver::formats::json::ValueBuilder reply;
                userver::formats::json::ValueBuilder values{userver::formats::json::Type::kObject};
                if (body.HasMember("ids")) {
                    for (const auto& id : body["ids"].As<std::vector<std::string>>()) {
                        if (current.Values.HasMember(id))
                            values[id] = current.Values[id];
                    }
                } else {
                    values = userver::formats::json::ValueBuilder{current.Values};
                }
                reply["configs"] = values.ExtractValue();
                // Протокол userver сравнивает timestamp лексикографически.
                std::ostringstream version;
                version << std::setw(20) << std::setfill('0') << current.Id;
                reply["updated_at"] = version.str();
                reply["revision"] = current.Id;
                return userver::formats::json::ToString(reply.ExtractValue());
            }
            platform::configs::ObjectKeys(
                body, name_ == "config-rollback"
                          ? std::set<std::string>{"expected_revision", "revision"}
                          : std::set<std::string>{"expected_revision", "configs"});
            const auto revision =
                name_ == "config-rollback"
                    ? store_.Publish(body["expected_revision"].As<std::int64_t>(),
                                     userver::formats::json::FromString("{}"),
                                     body["revision"].As<std::int64_t>())
                    : store_.Publish(body["expected_revision"].As<std::int64_t>(), body["configs"]);
            LOG_INFO() << "Config revision published"
                       << userver::logging::LogExtra{{"config_revision", revision.Id}};
            userver::formats::json::ValueBuilder reply;
            reply["revision"] = revision.Id;
            return userver::formats::json::ToString(reply.ExtractValue());
        } catch (const RevisionConflict&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
        } catch (const RevisionNotFound&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        } catch (const std::invalid_argument&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        } catch (const userver::formats::json::Exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        }
        return R"({"error":"Invalid config request or revision"})";
    }

  private:
    std::string name_;
    ConfigStore& store_;
};

}  // namespace configs
