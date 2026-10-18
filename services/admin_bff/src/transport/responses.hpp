#pragma once
#include <userver/formats/json.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
namespace admin_bff::detail {
using Http = userver::server::http::HttpRequest;
inline std::string JsonError(const Http& http, int status, const std::string& error,
                             const std::string& message) {
    http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(status));
    userver::formats::json::ValueBuilder body;
    body["error"] = error;
    body["message"] = message;
    return userver::formats::json::ToString(body.ExtractValue());
}
inline std::string Redirect(const Http& http, const std::string& target) {
    http.SetResponseStatus(userver::server::http::HttpStatus::kFound);
    http.GetHttpResponse().SetHeader(std::string_view{"Location"}, target);
    return {};
}
}  // namespace admin_bff::detail
