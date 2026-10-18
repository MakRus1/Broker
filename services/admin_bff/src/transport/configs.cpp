#include <transport/admin.hpp>
#include <transport/responses.hpp>
#include <userver/server/http/http_response.hpp>

namespace admin_bff {
using Http = userver::server::http::HttpRequest;
using detail::JsonError;

std::string Admin::Forward(const std::string& path, const userver::formats::json::Value& body,
    const std::string& token, const Http& http) const {
    namespace api = clients::config_admin;
    if (path == "/api/configs/current" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsCurrent(
            {.body = api::AdminConfigsCurrent::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsCurrent::ToJson(result);
    }
    if (path == "/api/configs/drafts/approve" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsApprove(
            {.body = api::AdminConfigsDraftsApprove::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsApprove::ToJson(result);
    }
    if (path == "/api/configs/drafts/cancel" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsCancel(
            {.body = api::AdminConfigsDraftsCancel::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsCancel::ToJson(result);
    }
    if (path == "/api/configs/drafts/create" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsCreate(
            {.body = api::AdminConfigsDraftsCreate::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(201));
        return api::AdminConfigsDraftsCreate::ToJson(result);
    }
    if (path == "/api/configs/drafts/edit" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsEdit(
            {.body = api::AdminConfigsDraftsEdit::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsEdit::ToJson(result);
    }
    if (path == "/api/configs/drafts/events" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsEvents(
            {.body = api::AdminConfigsDraftsEvents::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsEvents::ToJson(result);
    }
    if (path == "/api/configs/drafts/get" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsGet(
            {.body = api::AdminConfigsDraftsGet::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsGet::ToJson(result);
    }
    if (path == "/api/configs/drafts/list" && http.GetMethodStr() == "POST") {
        const auto result = deps_.config_admin.AdminConfigsDraftsList(
            {.body = api::AdminConfigsDraftsList::ParseBody(body)}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsDraftsList::ToJson(result);
    }
    if (path == "/api/configs/history" && http.GetMethodStr() == "GET") {
        const auto result = deps_.config_admin.AdminConfigsHistory({}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsHistory::ToJson(result);
    }
    if (path == "/api/configs/list" && http.GetMethodStr() == "GET") {
        const auto result = deps_.config_admin.AdminConfigsList({}, token);
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(200));
        return api::AdminConfigsList::ToJson(result);
    }
    return JsonError(http, 404, "not_found", "Метод не найден");
}
}  // namespace admin_bff
