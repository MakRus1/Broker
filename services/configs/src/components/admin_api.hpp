#pragma once
#include <components/config_store.hpp>
#include <optional>
#include <vector>

namespace configs::admin {

// Общий перевод доменных ошибок; контракты и view остаются отдельными для каждой ручки.
template <typename Response, typename BadRequest, typename Forbidden, typename NotFound,
          typename Conflict, typename Call, typename Convert>
Response Execute(Call&& call, Convert&& convert) {
    try {
        return convert(call());
    } catch (const DraftForbidden& error) {
        return Forbidden{.error = "forbidden", .message = error.what()};
    } catch (const RevisionConflict& error) {
        return Conflict{.error = "conflict", .message = error.what()};
    } catch (const RevisionNotFound& error) {
        return NotFound{.error = "not_found", .message = error.what()};
    } catch (const std::invalid_argument& error) {
        return BadRequest{.error = "invalid_request", .message = error.what()};
    } catch (const userver::formats::json::Exception& error) {
        return BadRequest{.error = "invalid_request", .message = error.what()};
    }
}

template <typename T>
std::optional<T> Optional(const platform::configs::Json& value, std::string_view key) {
    if (!value.HasMember(key) || value[key].IsNull())
        return std::nullopt;
    return value[key].As<T>();
}

inline std::vector<platform::configs::Json> Items(const platform::configs::Json& value) {
    std::vector<platform::configs::Json> result;
    for (const auto& item : value)
        result.push_back(item);
    return result;
}

}  // namespace configs::admin
