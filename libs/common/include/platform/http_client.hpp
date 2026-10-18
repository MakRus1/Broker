#pragma once

#include <charconv>
#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <userver/clients/http/component.hpp>
#include <userver/clients/http/error.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/http/url.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform::http {

class ApiError : public std::runtime_error {
  public:
    explicit ApiError(int status)
        : std::runtime_error("HTTP API error " + std::to_string(status)), status_(status) {}

    int GetStatus() const noexcept {
        return status_;
    }

  private:
    int status_;
};

class ContractError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Штатные исключения сохраняют различие таймаута, отмены и ошибки транспорта.
using TimeoutError = userver::clients::http::TimeoutException;
using CancelledError = userver::clients::http::CancelException;
using TransportError = userver::clients::http::BaseException;

struct Request {
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> query;
    std::optional<std::string> body;
    std::string content_type;
    std::string path_template;
    std::optional<std::string> bearer_token;
};

struct Response {
    int status;
    std::string content_type;
    std::string body;
};

class Transport {
  public:
    virtual ~Transport() = default;
    virtual Response Perform(const Request&) = 0;
};

template <typename T> std::string Scalar(const T& value) {
    if constexpr (std::is_enum_v<T>) {
        return Scalar(EnumValue(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else {
        char buffer[128];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
        if (result.ec != std::errc{})
            throw std::invalid_argument("Invalid scalar");
        return std::string(buffer, result.ptr);
    }
}

inline void ReplacePath(std::string& path, const std::string& placeholder,
                        const std::string& value) {
    // Каждое значение кодируется как один сегмент, включая слеш и знак вопроса.
    auto encoded = userver::http::UrlEncode(value);
    if (value == ".")
        encoded = "%2E";
    if (value == "..")
        encoded = "%2E%2E";
    path.replace(path.find(placeholder), placeholder.size(), encoded);
}

inline void RequireContentType(const Response& response, const std::string& expected) {
    auto media = response.content_type.substr(0, response.content_type.find(';'));
    while (!media.empty() && media.back() == ' ')
        media.pop_back();
    for (auto& ch : media) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    if (media != expected)
        throw ContractError("Unexpected HTTP response Content-Type");
}

struct ClientSettings {
    int Timeout;
    int MaxRetries;
};
using SettingsProvider = std::function<ClientSettings(const std::string&)>;

class HttpTransport final : public Transport {
  public:
    HttpTransport(const userver::components::ComponentConfig& config,
                  const userver::components::ComponentContext& context,
                  SettingsProvider settings = {})
        : client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
          base_url_(config["base-url"].As<std::string>()),
          timeout_(config["timeout-ms"].As<int>(1000)), destination_(config.Name()),
          settings_(std::move(settings)) {
        if ((base_url_.rfind("http://", 0) != 0 && base_url_.rfind("https://", 0) != 0) ||
            base_url_.find_first_of("?#@\r\n") != std::string::npos || timeout_.count() <= 0) {
            throw std::invalid_argument("Invalid HTTP client base-url or timeout-ms");
        }
        while (!base_url_.empty() && base_url_.back() == '/')
            base_url_.pop_back();
    }

    Response Perform(const Request& input) override {
        auto url = base_url_ + input.path;
        char separator = '?';
        for (const auto& [name, value] : input.query) {
            url +=
                separator + userver::http::UrlEncode(name) + "=" + userver::http::UrlEncode(value);
            separator = '&';
        }
        const auto settings = settings_ ? settings_(input.path_template)
                                        : ClientSettings{static_cast<int>(timeout_.count()), 0};
        // Повторы разрешены только для безопасных методов; POST не становится идемпотентным из-за
        // конфига.
        const bool safe =
            input.method == "GET" || input.method == "HEAD" || input.method == "OPTIONS";
        auto request = client_.CreateRequest();
        request.url(std::move(url))
            .method(Method(input.method))
            .timeout(std::chrono::milliseconds(settings.Timeout))
            .retry(static_cast<short>(1 + (safe ? settings.MaxRetries : 0)), true)
            .follow_redirects(false)
            .SetLoggedUrl(input.path_template)
            .SetUrlTemplate(input.path_template)
            .SetDestinationMetricName(destination_ + "." + input.path_template);
        userver::clients::http::Headers headers;
        if (input.bearer_token)
            headers.emplace(std::string_view{"Authorization"}, "Bearer " + *input.bearer_token);
        if (input.body) {
            request.data(*input.body);
            headers.emplace(std::string_view{"Content-Type"}, input.content_type);
            if (input.method == "GET") {
                // Установка тела переключает libcurl на POST; сохраняем явный GET.
                request.set_custom_http_request_method("GET");
            }
        }
        request.headers(headers);
        // userver передает текущий контекст трассы, дедлайн и отмену задачи.
        // Статусы API не превращаем в исключения транспорта через raise_for_status.
        const auto response = request.perform();
        const auto& response_headers = response->headers();
        const auto content_type = response_headers.find(std::string_view{"Content-Type"});
        return {static_cast<int>(response->status_code()),
                content_type == response_headers.end() ? std::string{} : content_type->second,
                response->body()};
    }

  private:
    static userver::clients::http::HttpMethod Method(const std::string& value) {
        using Method = userver::clients::http::HttpMethod;
        if (value == "GET")
            return Method::kGet;
        if (value == "POST")
            return Method::kPost;
        if (value == "PUT")
            return Method::kPut;
        if (value == "PATCH")
            return Method::kPatch;
        if (value == "DELETE")
            return Method::kDelete;
        if (value == "HEAD")
            return Method::kHead;
        if (value == "OPTIONS")
            return Method::kOptions;
        throw std::invalid_argument("Unsupported HTTP method");
    }

    userver::clients::http::Client& client_;
    std::string base_url_;
    std::chrono::milliseconds timeout_;
    std::string destination_;
    SettingsProvider settings_;
};

inline userver::yaml_config::Schema ClientConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Типизированный HTTP-клиент сервиса
additionalProperties: false
properties:
    base-url:
        type: string
        description: Базовый URL сервиса из настроек окружения
    timeout-ms:
        type: integer
        description: Таймаут запроса в миллисекундах, ограничивается входящим дедлайном
        minimum: 1
)");
}

}  // namespace platform::http
