#include <components/sessions.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
using userver::http::UrlEncode;
using HttpClient = userver::components::HttpClient;

std::string Random() {
    std::string bytes(32, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), bytes.size()) != 1)
        throw std::runtime_error("Random generation failed");
    return userver::crypto::base64::Base64UrlEncode(bytes, userver::crypto::base64::Pad::kWithout);
}

std::string Challenge(const std::string& verifier) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(verifier.data()), verifier.size(), digest);
    return userver::crypto::base64::Base64UrlEncode(
        std::string(reinterpret_cast<char*>(digest), sizeof(digest)),
        userver::crypto::base64::Pad::kWithout);
}
}  // namespace

namespace admin_bff {
Sessions::Sessions(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), Origin(config["origin"].As<std::string>()),
      auth_(context.FindComponent<platform::auth::Component>()),
      http_(context.FindComponent<HttpClient>("auth-http-client").GetHttpClient()),
      issuer_(config["issuer"].As<std::string>()),
      token_url_(config["token-url"].As<std::string>()),
      revoke_url_(config["revoke-url"].As<std::string>()),
      client_(config["client-id"].As<std::string>()),
      secure_(config["secure-cookie"].As<bool>(true)) {
    if (Origin.empty() || Origin.back() == '/' || client_.empty() ||
        (secure_ &&
            (!Origin.starts_with("https://") || !issuer_.starts_with("https://") ||
                !token_url_.starts_with("https://") || !revoke_url_.starts_with("https://"))) ||
        (!secure_ && !Origin.starts_with("http://localhost:")))
        throw std::invalid_argument("Invalid admin origin or TLS configuration");
}

void Sessions::Cleanup() {
    const auto now = Clock::now();
    std::erase_if(pending_, [now](const auto& pair) { return pair.second.Expires <= now; });
    std::erase_if(sessions_, [now](const auto& pair) { return pair.second->Expires <= now; });
}

std::pair<std::string, std::string> Sessions::Begin() {
    const auto state = Random();
    const Pending pending{Random(), Random(), Clock::now() + std::chrono::minutes{5}};
    {
        std::lock_guard lock(mutex_);
        Cleanup();
        if (pending_.size() >= 1000)
            throw CapacityExceeded{};
        pending_.emplace(state, pending);
    }
    const auto url = issuer_ +
        "/protocol/openid-connect/auth?response_type=code&scope=openid%20profile&client_id=" +
        UrlEncode(client_) + "&redirect_uri=" + UrlEncode(Origin + "/auth/callback") +
        "&state=" + UrlEncode(state) + "&nonce=" + UrlEncode(pending.Nonce) +
        "&code_challenge_method=S256&code_challenge=" + UrlEncode(Challenge(pending.Verifier));
    return {state, url};
}

userver::formats::json::Value Sessions::Exchange(const std::string& form) const {
    auto request = http_.CreateRequest();
    request.post(token_url_, form)
        .headers({{"Content-Type", "application/x-www-form-urlencoded"}})
        .timeout(std::chrono::seconds{5})
        .retry(1)
        .follow_redirects(false)
        .SetLoggedUrl("oidc-token");
    const auto response = request.perform();
    // Не включаем тело ответа провайдера в исключения и логи.
    if (response->status_code() >= 500)
        throw platform::auth::Unavailable{};
    if (response->status_code() != 200)
        throw userver::server::handlers::Unauthorized{};
    return userver::formats::json::FromString(response->body());
}

void Sessions::Tokens(Session& session, const userver::formats::json::Value& tokens) {
    const auto expires = tokens["expires_in"].As<int>();
    const auto access = tokens["access_token"].As<std::string>();
    const auto refresh = tokens["refresh_token"].As<std::string>();
    if (expires <= 0 || expires > 86400 || access.empty() || refresh.empty())
        throw userver::server::handlers::Unauthorized{};
    session.Access = access;
    session.Refresh = refresh;
    session.TokenExpires = Clock::now() + std::chrono::seconds{expires};
}

std::string Sessions::Complete(const std::string& state, const std::string& code) {
    Pending pending;
    {
        std::lock_guard lock(mutex_);
        Cleanup();
        const auto found = pending_.find(state);
        if (found == pending_.end())
            throw userver::server::handlers::Unauthorized{};
        pending = found->second;
        pending_.erase(found);  // Authorization code и state используются только один раз.
    }
    const auto tokens = Exchange("grant_type=authorization_code&client_id=" + UrlEncode(client_) +
        "&redirect_uri=" + UrlEncode(Origin + "/auth/callback") + "&code=" + UrlEncode(code) +
        "&code_verifier=" + UrlEncode(pending.Verifier));
    auto session = std::make_shared<Session>();
    try {
        session->User = auth_.VerifyLogin(tokens["access_token"].As<std::string>(),
            tokens["id_token"].As<std::string>(), client_, pending.Nonce);
    } catch (const platform::auth::Unavailable&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception&) {
        throw userver::server::handlers::Unauthorized{};
    }
    Tokens(*session, tokens);
    session->Csrf = Random();
    session->Expires = Clock::now() + std::chrono::hours{8};
    const auto id = Random();
    std::lock_guard lock(mutex_);
    Cleanup();
    if (sessions_.size() >= 2000)
        throw CapacityExceeded{};
    sessions_.emplace(id, std::move(session));
    return id;
}

std::shared_ptr<Session> Sessions::Get(const std::string& id) {
    std::lock_guard lock(mutex_);
    const auto found = sessions_.find(id);
    if (found == sessions_.end())
        throw userver::server::handlers::Unauthorized{};
    if (found->second->Expires <= Clock::now()) {
        sessions_.erase(found);
        throw userver::server::handlers::Unauthorized{};
    }
    return found->second;
}

void Sessions::EnsureFresh(Session& session) {
    if (session.Expires <= Clock::now())
        throw userver::server::handlers::Unauthorized{};
    if (session.TokenExpires <= Clock::now() + std::chrono::seconds{30}) {
        const auto tokens = Exchange("grant_type=refresh_token&client_id=" + UrlEncode(client_) +
            "&refresh_token=" + UrlEncode(session.Refresh));
        platform::auth::Principal principal;
        try {
            principal = auth_.VerifyAccess(tokens["access_token"].As<std::string>());
        } catch (const platform::auth::Unavailable&) {
            throw;
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            throw userver::server::handlers::Unauthorized{};
        }
        if (principal.ActorId() != session.User.ActorId())
            throw userver::server::handlers::Unauthorized{};
        Tokens(session, tokens);
        session.User = principal;
    }
}

void Sessions::Logout(const std::string& id) {
    auto session = Get(id);
    {
        std::lock_guard lock(mutex_);
        sessions_.erase(id);
    }
    std::lock_guard lock(session->Mutex);
    try {
        const auto form = "client_id=" + UrlEncode(client_) +
            "&token=" + UrlEncode(session->Refresh) + "&token_type_hint=refresh_token";
        auto request = http_.CreateRequest();
        request.post(revoke_url_, form)
            .headers({{"Content-Type", "application/x-www-form-urlencoded"}})
            .timeout(std::chrono::seconds{2})
            .retry(1)
            .follow_redirects(false)
            .SetLoggedUrl("oidc-revoke");
        (void)request.perform();
    } catch (const std::exception&) { /* Локальная сессия уже уничтожена; токен не логируем. */
    }
}

userver::server::http::Cookie Sessions::Cookie(
    const std::string& name, const std::string& value, int seconds) const {
    userver::server::http::Cookie cookie{CookieName(name), value};
    cookie.SetPath("/").SetHttpOnly().SetSameSite("Lax").SetMaxAge(std::chrono::seconds{seconds});
    if (secure_)
        cookie.SetSecure();
    return cookie;
}

std::string Sessions::CookieName(const std::string& name) const {
    return (secure_ ? "__Host-" : "") + name;
}

std::string Sessions::LogoutUrl() const {
    return issuer_ + "/protocol/openid-connect/logout?client_id=" + UrlEncode(client_) +
        "&post_logout_redirect_uri=" + UrlEncode(Origin + "/");
}

userver::yaml_config::Schema Sessions::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<ComponentBase>(R"(
type: object
description: Серверные сессии административного BFF
additionalProperties: false
properties:
    origin: {type: string, description: Публичный origin админки}
    issuer: {type: string, description: Доверенный issuer Keycloak}
    token-url: {type: string, description: Внутренний адрес обмена токенов}
    revoke-url: {type: string, description: Внутренний адрес отзыва токенов}
    client-id: {type: string, description: OIDC клиент}
    secure-cookie: {type: boolean, description: HTTPS cookie вне localhost, default: true}
)");
}
}  // namespace admin_bff
