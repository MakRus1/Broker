#include <transport/admin.hpp>
#include <transport/responses.hpp>
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
    const auto type = path.ends_with(".js")    ? "application/javascript"
                      : path.ends_with(".css") ? "text/css"
                                               : "text/html; charset=utf-8";
    response.SetHeader(std::string_view{"Content-Type"}, type);
    return std::get<std::string>(file->data_or_path);
}
}  // namespace admin_bff
