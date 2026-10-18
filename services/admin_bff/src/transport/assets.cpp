#include <openssl/rand.h>
#include <stdexcept>
#include <transport/admin.hpp>
#include <transport/responses.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/server/http/http_response.hpp>

namespace admin_bff {
using Http = userver::server::http::HttpRequest;
using detail::JsonError;

std::string Admin::ServeAssets(const Http& http) const {
    auto& response = http.GetHttpResponse();
    const auto path = http.GetRequestPath();
    const auto file = assets_.TryGetFile(path == "/" ? "/index.html" : path);
    if (!file)
        return JsonError(http, 404, "not_found", "Страница не найдена");
    const auto type = path.ends_with(".js") ? "application/javascript"
        : path.ends_with(".css")            ? "text/css"
                                            : "text/html; charset=utf-8";
    response.SetHeader(std::string_view{"Content-Type"}, type);
    auto body = std::get<std::string>(file->data_or_path);
    if (path == "/" || path == "/index.html") {
        std::string bytes(32, '\0');
        if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), bytes.size()) != 1)
            throw std::runtime_error("Nonce generation failed");
        const auto nonce =
            userver::crypto::base64::Base64UrlEncode(bytes, userver::crypto::base64::Pad::kWithout);
        constexpr std::string_view marker = "__BROKER_CSP_NONCE__";
        if (const auto at = body.find(marker); at != std::string::npos)
            body.replace(at, marker.size(), nonce);
        response.SetHeader(
            std::string_view{"Content-Security-Policy"}, detail::ContentSecurityPolicy(nonce));
    }
    return body;
}
}  // namespace admin_bff
