#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <map>
#include <platform/auth/component.hpp>
#include <platform/auth/verifier.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/handler_config.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/utils/periodic_task.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <variant>

namespace {
using Clock = std::chrono::steady_clock;

class Middleware final : public userver::server::middlewares::HttpMiddlewareBase {
  public:
    Middleware(const platform::auth::Component& auth, std::string path)
        : auth_(auth), path_(std::move(path)) {}

  private:
    void HandleRequest(userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override {
        if (!auth_.IsProtected(path_)) {
            Next(request, context);
            return;
        }
        int status = 401;
        std::string error = "unauthorized";
        try {
            context.SetData(
                "platform-auth", auth_.Authorize(path_, request.GetHeader("Authorization")));
        } catch (const platform::auth::Unavailable&) {
            status = 503;
            error = "auth_unavailable";
        } catch (const platform::auth::Forbidden&) {
            status = 403;
            error = "forbidden";
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            // Ни токен, ни диагностические детали его разбора не попадают в ответ и логи.
        }
        if (context.GetDataOptional<platform::auth::Identity>("platform-auth")) {
            Next(request, context);
            return;
        }
        auto& response = request.GetHttpResponse();
        response.SetStatus(static_cast<userver::server::http::HttpStatus>(status));
        response.SetHeader(std::string_view{"Content-Type"}, "application/json");
        response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
        if (status == 401)
            response.SetHeader(std::string_view{"WWW-Authenticate"}, "Bearer");
        userver::formats::json::ValueBuilder body;
        body["error"] = error;
        body["message"] = status == 503 ? "Authentication unavailable"
            : status == 403             ? "Forbidden"
                                        : "Unauthorized";
        response.SetData(userver::formats::json::ToString(body.ExtractValue()));
    }

    const platform::auth::Component& auth_;
    const std::string path_;
};
}  // namespace

namespace platform::auth {
struct Component::State {
    struct Cached {
        Verifier Check;
        Clock::time_point Received;

        Cached(const Settings& settings, const std::string& body)
            : Check(settings, body), Received(Clock::now()) {}
    };

    userver::clients::http::Client& Client;
    Settings Config;
    std::string Url;
    std::string Prefix;
    std::map<std::string, std::string> Permissions;
    std::chrono::seconds MaxAge;
    // Атомарные операции со shared_ptr поддерживаются и libc++ без atomic<shared_ptr>.
    std::shared_ptr<const Cached> Cache;
    std::atomic<std::int64_t> LastForced{0};
    userver::utils::PeriodicTask RefreshTask;

    State(
        userver::clients::http::Client& client, const userver::components::ComponentConfig& config)
        : Client(client),
          Config{config["issuer"].As<std::string>(), config["audience"].As<std::string>(),
              config["roles-client"].As<std::string>()},
          Url(config["jwks-url"].As<std::string>()),
          Prefix(config["protected-prefix"].As<std::string>()),
          Permissions(config["permissions"].As<std::map<std::string, std::string>>()),
          MaxAge(config["max-key-age-seconds"].As<int>(300)) {
        const bool insecure = config["allow-http"].As<bool>(false);
        auto valid = [insecure](const std::string& url) {
            return url.starts_with("https://") || (insecure && url.starts_with("http://"));
        };
        if (!valid(Config.Issuer) || !valid(Url) || Config.Issuer.find('#') != std::string::npos ||
            Config.Issuer.size() > 1024 || Config.Audience.empty() || Config.RolesClient.empty() ||
            Prefix.empty() || Prefix.front() != '/' || Prefix.back() != '/' ||
            MaxAge < std::chrono::seconds{30} || MaxAge > std::chrono::hours{24})
            throw std::invalid_argument("Invalid authentication configuration");
        for (const auto& [path, permission] : Permissions)
            if (!path.starts_with(Prefix) || permission.empty())
                throw std::invalid_argument("Invalid permission policy");
    }

    void Refresh() {
        try {
            const auto response = Client.CreateRequest()
                                      .get(Url)
                                      .timeout(std::chrono::seconds{2})
                                      .retry(1)
                                      .follow_redirects(false)
                                      .SetLoggedUrl("jwks")
                                      .perform();
            response->raise_for_status();
            std::atomic_store(&Cache, std::make_shared<const Cached>(Config, response->body()));
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            LOG_WARNING() << "Authentication key refresh failed";
        }
    }

    void RefreshUnknownKey() {
        const auto now =
            std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch())
                .count();
        auto previous = LastForced.load();
        if (now - previous >= 5 && LastForced.compare_exchange_strong(previous, now))
            RefreshTask.ForceStepAsync();
    }
};

Component::Component(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpMiddlewareFactoryBase(config, context),
      state_(std::make_unique<State>(
          context.FindComponent<userver::components::HttpClient>("auth-http-client")
              .GetHttpClient(),
          config)) {
    state_->Refresh();
    state_->RefreshTask.Start("auth-jwks", std::chrono::seconds{15}, [this] { state_->Refresh(); });
}

Component::~Component() {
    state_->RefreshTask.Stop();
}

bool Component::IsProtected(std::string_view path) const {
    return path.starts_with(state_->Prefix);
}

Identity Component::Authorize(const std::string& path, const std::string& authorization) const {
    if (authorization.size() < 8 ||
        !std::equal(authorization.begin(), authorization.begin() + 7, "bearer ",
            [](unsigned char a, unsigned char b) { return std::tolower(a) == b; }))
        throw std::invalid_argument("Missing bearer token");
    const auto identity = VerifyAccess(authorization.substr(7));
    const auto permission = state_->Permissions.find(path);
    if (permission == state_->Permissions.end() ||
        !identity.Permissions.contains(permission->second))
        throw Forbidden{};
    return identity;
}

Identity Component::VerifyAccess(const std::string& token) const {
    const auto cached = std::atomic_load(&state_->Cache);
    if (!cached || Clock::now() - cached->Received > state_->MaxAge)
        throw Unavailable{};
    Identity identity;
    try {
        identity = cached->Check.Verify(token);
    } catch (const UnknownKey&) {
        state_->RefreshUnknownKey();
        throw;
    }
    return identity;
}

Identity Component::VerifyLogin(const std::string& access, const std::string& id_token,
    const std::string& client, const std::string& nonce) const {
    const auto cached = std::atomic_load(&state_->Cache);
    if (!cached || Clock::now() - cached->Received > state_->MaxAge)
        throw Unavailable{};
    try {
        return cached->Check.VerifyLogin(access, id_token, client, nonce);
    } catch (const UnknownKey&) {
        state_->RefreshUnknownKey();
        throw;
    }
}

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Component::Create(
    const userver::server::handlers::HttpHandlerBase& handler,
    userver::yaml_config::YamlConfig) const {
    // Политика относится к шаблону маршрута, а не к значениям path-параметров.
    const auto* path = std::get_if<std::string>(&handler.GetConfig().path);
    return std::make_unique<Middleware>(*this, path ? *path : std::string{});
}

userver::yaml_config::Schema Component::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<HttpMiddlewareFactoryBase>(R"(
type: object
description: Проверка пользовательских access token и разрешений
additionalProperties: false
properties:
    issuer: {type: string, description: Доверенный издатель}
    audience: {type: string, description: Получатель токена}
    roles-client: {type: string, description: Клиент с ролями API}
    jwks-url: {type: string, description: Доверенный адрес публичных ключей}
    protected-prefix: {type: string, description: Защищаемый префикс}
    permissions:
        type: object
        description: Требуемое разрешение для каждого пути
        properties: {}
        additionalProperties: {type: string, description: Необходимое разрешение}
    allow-http: {type: boolean, description: HTTP только в локальном окружении, default: false}
    max-key-age-seconds: {type: integer, description: Максимальный возраст кеша ключей, default: 300}
)");
}
}  // namespace platform::auth
