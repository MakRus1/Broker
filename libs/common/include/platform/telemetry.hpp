#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <platform/readiness.hpp>
#include <stdexcept>
#include <string>

#include <userver/components/component_config.hpp>
#include <userver/server/middlewares/builtin.hpp>
#include <userver/server/middlewares/configuration.hpp>
#include <userver/server/middlewares/http_middleware_base.hpp>
#include <userver/tracing/span.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform {

struct ServiceMetadata {
    std::string name;
    std::string environment;

    void Apply(userver::tracing::Span& span) const {
        span.AddTag("service.name", name);
        span.AddTag("service.version", std::string(PLATFORM_VERSION));
        span.AddTag("deployment.environment.name", environment);
    }
};

class HttpTelemetry final : public userver::server::middlewares::HttpMiddlewareBase {
  public:
    explicit HttpTelemetry(ServiceMetadata metadata) : metadata_(std::move(metadata)) {}

  private:
    void HandleRequest(userver::server::http::HttpRequest& request,
                       userver::server::request::RequestContext& context) const override {
        metadata_.Apply(userver::tracing::Span::CurrentSpan());
        Next(request, context);
    }

    const ServiceMetadata metadata_;
};

class HttpTelemetryFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "platform-http-telemetry";

    HttpTelemetryFactory(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
        : HttpMiddlewareFactoryBase(config, context),
          metadata_{config["service-name"].As<std::string>(),
                    config["environment"].As<std::string>()} {}

    const ServiceMetadata& GetMetadata() const {
        return metadata_;
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<HttpMiddlewareFactoryBase>(R"(
type: object
description: Метаданные сервиса для всех HTTP и gRPC запросов
additionalProperties: false
properties:
    service-name:
        type: string
        description: Имя сервиса
    environment:
        type: string
        description: Окружение развертывания
)");
    }

  private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase>
    Create(const userver::server::handlers::HttpHandlerBase&,
           userver::yaml_config::YamlConfig) const override {
        return std::make_unique<HttpTelemetry>(metadata_);
    }

    const ServiceMetadata metadata_;
};

class HttpTelemetryPipeline final : public userver::server::middlewares::PipelineBuilder {
  public:
    static constexpr std::string_view kName = "platform-http-pipeline";
    HttpTelemetryPipeline(const userver::components::ComponentConfig& config,
                          const userver::components::ComponentContext& context)
        : PipelineBuilder(config, context),
          readiness_enabled_(context.FindComponentOptional<HttpReadinessFactory>() != nullptr) {}

    userver::server::middlewares::MiddlewaresList
    BuildPipeline(userver::server::middlewares::MiddlewaresList pipeline) const override {
        pipeline = PipelineBuilder::BuildPipeline(std::move(pipeline));
        const auto tracing = std::find(pipeline.begin(), pipeline.end(),
                                       userver::server::middlewares::builtin::kTracing);
        // Контекст уже создан, а аутентификация и обработчик еще не запущены.
        if (tracing == pipeline.end())
            throw std::logic_error("HTTP tracing middleware is required");
        const auto position = std::distance(pipeline.begin(), tracing) + 1;
        pipeline.insert(pipeline.begin() + position, std::string(HttpTelemetryFactory::kName));
        if (readiness_enabled_)
            pipeline.insert(pipeline.begin() + position + 1,
                            std::string(HttpReadinessFactory::kName));
        return pipeline;
    }

  private:
    const bool readiness_enabled_;
};

}  // namespace platform

template <>
inline constexpr bool userver::components::kHasValidate<platform::HttpTelemetryFactory> = true;
template <>
inline constexpr bool userver::components::kHasValidate<platform::HttpTelemetryPipeline> = true;
