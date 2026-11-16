#include <browser_storage/sql_queries.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <platform/browser/sessions.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
using userver::http::UrlEncode;
using HttpClient = userver::components::HttpClient;
namespace pg = userver::storages::postgres;
namespace sql = browser_storage::sql;
constexpr std::int64_t kMaxPendingLogins = 1000;
constexpr std::int64_t kMaxSessions = 2000;
constexpr auto kTokenRefreshLeadTime = std::chrono::seconds{30};

struct StoredSession {
    std::string Actor, Access, Refresh, Csrf;
    pg::TimePointTz TokenExpires, Expires;
};

bool NeedsRefresh(platform::browser::Clock::time_point expires) {
    return expires <= platform::browser::Clock::now() + kTokenRefreshLeadTime;
}

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

namespace platform::browser {
Sessions::Sessions(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), Origin(config["origin"].As<std::string>()),
      SessionCookie(config["session-cookie"].As<std::string>("broker_session")),
      LoginCookie(config["login-cookie"].As<std::string>("broker_login")),
      auth_(context.FindComponent<platform::auth::Component>()),
      http_(context.FindComponent<HttpClient>("auth-http-client").GetHttpClient()),
      issuer_(config["issuer"].As<std::string>()),
      token_url_(config["token-url"].As<std::string>()),
      revoke_url_(config["revoke-url"].As<std::string>()),
      client_(config["client-id"].As<std::string>()),
      secure_(config["secure-cookie"].As<bool>(true)) {
    if (const auto postgres = config["postgres-component"].As<std::optional<std::string>>())
        cluster_ = context.FindComponent<userver::components::Postgres>(*postgres).GetCluster();
    if (SessionCookie.empty() || LoginCookie.empty() || SessionCookie == LoginCookie ||
        Origin.empty() || Origin.back() == '/' || client_.empty() ||
        (secure_ &&
            (!Origin.starts_with("https://") || !issuer_.starts_with("https://") ||
                !token_url_.starts_with("https://") || !revoke_url_.starts_with("https://"))) ||
        (!secure_ && !Origin.starts_with("http://localhost:")))
        throw std::invalid_argument("Invalid browser origin or TLS configuration");

    if (cluster_) {
        cleanup_task_.Start("browser-session-cleanup",
            userver::utils::PeriodicTask::Settings{std::chrono::minutes{1}}, [this] {
                cluster_->Execute(pg::ClusterHostType::kMaster, sql::kClearExpiredLogins);
                cluster_->Execute(pg::ClusterHostType::kMaster, sql::kClearExpiredSessions);
            });
    }
}

void Sessions::Cleanup() {
    const auto now = Clock::now();
    std::erase_if(pending_, [now](const auto& pair) { return pair.second.Expires <= now; });
    std::erase_if(sessions_, [now](const auto& pair) { return pair.second->Expires <= now; });
}

std::pair<std::string, std::string> Sessions::Begin() {
    const auto state = Random();
    const Pending pending{Random(), Random(), Clock::now() + kLoginTtl};
    if (cluster_) {
        StorePending(state, pending);
    } else {
        std::lock_guard lock(mutex_);
        Cleanup();
        if (pending_.size() >= static_cast<std::size_t>(kMaxPendingLogins))
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
    const auto pending = ConsumePending(state);
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
    session->Expires = Clock::now() + kSessionTtl;
    const auto id = Random();
    if (cluster_) {
        StoreSession(id, *session);
    } else {
        std::lock_guard lock(mutex_);
        Cleanup();
        if (sessions_.size() >= static_cast<std::size_t>(kMaxSessions))
            throw CapacityExceeded{};
        sessions_.emplace(id, std::move(session));
    }
    return id;
}

std::shared_ptr<Session> Sessions::GetLocal(const std::string& id) {
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

SessionSnapshot Sessions::GetAuthenticated(const std::string& id) {
    if (id.empty())
        throw userver::server::handlers::Unauthorized{};
    if (cluster_)
        return GetStored(id);

    const auto session = GetLocal(id);
    std::lock_guard lock(session->Mutex);
    if (session->Expires <= Clock::now())
        throw userver::server::handlers::Unauthorized{};
    if (NeedsRefresh(session->TokenExpires))
        Refresh(*session, session->User.ActorId());
    return {session->User, session->Access, session->Csrf};
}

platform::auth::Identity Sessions::VerifyAccess(const std::string& token) const try {
    return auth_.VerifyAccess(token);
} catch (const platform::auth::Unavailable&) {

    throw;
} catch (const std::bad_alloc&) {
    throw;
} catch (const std::exception&) {
    throw userver::server::handlers::Unauthorized{};
}

void Sessions::Refresh(Session& session, const std::string& actor) {
    const auto tokens = Exchange("grant_type=refresh_token&client_id=" + UrlEncode(client_) +
        "&refresh_token=" + UrlEncode(session.Refresh));
    auto identity = VerifyAccess(tokens["access_token"].As<std::string>());
    if (identity.ActorId() != actor)
        throw userver::server::handlers::Unauthorized{};
    Tokens(session, tokens);
    session.User = std::move(identity);
}

void Sessions::StorePending(const std::string& state, const Pending& pending) try {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSessionCapacity, std::string{"browser_login_attempts"});
    transaction.Execute(sql::kClearExpiredLogins);
    if (transaction
            .Execute(sql::kCreatePendingLogin, Challenge(state), pending.Verifier, pending.Nonce,
                pg::TimePointTz{pending.Expires}, kMaxPendingLogins)
            .IsEmpty())
        throw CapacityExceeded{};
    transaction.Commit();
} catch (const pg::Error&) {

    throw platform::auth::Unavailable{};
}

Pending Sessions::ConsumePending(const std::string& state) try {
    if (cluster_) {
        const auto rows = cluster_->Execute(
            pg::ClusterHostType::kMaster, sql::kConsumePendingLogin, Challenge(state));
        if (rows.IsEmpty())
            throw userver::server::handlers::Unauthorized{};

        struct LoginRow {
            std::string Verifier, Nonce;
        };

        auto login = rows.AsSingleRow<LoginRow>(pg::kRowTag);
        return {std::move(login.Verifier), std::move(login.Nonce), {}};
    }

    std::lock_guard lock(mutex_);
    Cleanup();
    auto login = pending_.extract(state);
    if (login.empty())
        throw userver::server::handlers::Unauthorized{};
    return std::move(login.mapped());
} catch (const pg::Error&) {

    throw platform::auth::Unavailable{};
}

void Sessions::StoreSession(const std::string& id, const Session& session) try {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSessionCapacity, std::string{"browser_sessions"});
    transaction.Execute(sql::kClearExpiredSessions);
    if (transaction
            .Execute(sql::kCreateSession, Challenge(id), session.User.ActorId(), session.Access,
                session.Refresh, session.Csrf, pg::TimePointTz{session.TokenExpires},
                pg::TimePointTz{session.Expires}, kMaxSessions)
            .IsEmpty())
        throw CapacityExceeded{};
    transaction.Commit();
} catch (const pg::Error&) {

    throw platform::auth::Unavailable{};
}

SessionSnapshot Sessions::GetStored(const std::string& id) try {
    const auto key = Challenge(id);
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kReadSession, key);
    if (rows.IsEmpty())
        throw userver::server::handlers::Unauthorized{};
    auto stored = rows.AsSingleRow<StoredSession>(pg::kRowTag);
    if (!NeedsRefresh(stored.TokenExpires))
        return {VerifyAccess(stored.Access), std::move(stored.Access), std::move(stored.Csrf)};

    // Ожидание блокировки может включать обмен токенов на другой реплике.
    const pg::CommandControl refresh_timeout{std::chrono::seconds{7}, std::chrono::seconds{6}};
    auto transaction =
        cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{}, refresh_timeout);
    const auto locked = transaction.Execute(sql::kLockSession, key);
    if (locked.IsEmpty())
        throw userver::server::handlers::Unauthorized{};
    stored = locked.AsSingleRow<StoredSession>(pg::kRowTag);
    if (stored.Expires.GetUnderlying() <= Clock::now())
        throw userver::server::handlers::Unauthorized{};
    if (!NeedsRefresh(stored.TokenExpires)) {
        transaction.Commit();
        return {VerifyAccess(stored.Access), std::move(stored.Access), std::move(stored.Csrf)};
    }

    Session session;
    session.Refresh = std::move(stored.Refresh);
    Refresh(session, stored.Actor);
    transaction.Execute(sql::kUpdateSessionTokens, key, session.Access, session.Refresh,
        pg::TimePointTz{session.TokenExpires});
    transaction.Commit();
    return {std::move(session.User), std::move(session.Access), std::move(stored.Csrf)};
} catch (const pg::Error&) {

    throw platform::auth::Unavailable{};
}

std::string Sessions::RemoveSession(const std::string& id) try {
    if (cluster_) {
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kDeleteSession, Challenge(id));
        if (rows.IsEmpty())
            throw userver::server::handlers::Unauthorized{};
        return rows.AsSingleRow<std::string>();
    }

    auto session = GetLocal(id);
    {
        std::lock_guard lock(mutex_);
        sessions_.erase(id);
    }
    std::lock_guard lock(session->Mutex);
    return std::move(session->Refresh);
} catch (const pg::Error&) {

    throw platform::auth::Unavailable{};
}

void Sessions::Logout(const std::string& id) {
    const auto refresh = RemoveSession(id);
    try {
        const auto form = "client_id=" + UrlEncode(client_) + "&token=" + UrlEncode(refresh) +
            "&token_type_hint=refresh_token";
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
description: Серверные сессии браузерного BFF
additionalProperties: false
properties:
    postgres-component: {type: string, description: Компонент общего PostgreSQL-хранилища сессий}
    session-cookie: {type: string, description: Имя cookie сессии, default: broker_session}
    login-cookie: {type: string, description: Имя cookie входа, default: broker_login}
    origin: {type: string, description: Публичный origin приложения}
    issuer: {type: string, description: Доверенный issuer Keycloak}
    token-url: {type: string, description: Внутренний адрес обмена токенов}
    revoke-url: {type: string, description: Внутренний адрес отзыва токенов}
    client-id: {type: string, description: OIDC клиент}
    secure-cookie: {type: boolean, description: HTTPS cookie вне localhost, default: true}
)");
}
}  // namespace platform::browser
