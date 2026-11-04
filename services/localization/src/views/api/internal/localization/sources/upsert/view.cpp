#include <helpers/editor.hpp>
#include <map>
#include <views/api/internal/localization/sources/upsert/view.hpp>

namespace localization::views::api::internal::localization::sources::upsert {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::localization::helpers;
    const auto collect = [](const auto& entries) {
        std::map<std::string, std::string> result;
        for (const auto& entry : entries) {
            if (!std::regex_match(entry.key, std::regex{"[A-Za-z0-9_.-]{1,200}"}) ||
                entry.value.empty() || entry.value.size() > 10000 ||
                !result.emplace(entry.key, entry.value).second)
                throw ApiError<400>{"invalid_source", "Некорректный исходный каталог"};
        }
        return result;
    };
    const auto ru = collect(request.body.ru);
    const auto en = collect(request.body.en);
    if (ru.empty() || ru.size() != en.size())
        throw ApiError<400>{"invalid_source", "Наборы ключей ru/en должны совпадать"};
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockSource);
    const auto current = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    json::ValueBuilder rows{json::Type::kArray};
    for (const auto& [key, text] : ru) {
        const auto english = en.find(key);
        const auto names = Parameters(text);
        if (english == en.end() || names != Parameters(english->second))
            throw ApiError<400>{"invalid_parameters", "Не совпадают параметры ru/en: " + key};
        const std::vector<std::string> parameters{names.begin(), names.end()};
        if (current["ru"].HasMember(key) &&
            Parameters(current["ru"][key].As<std::string>()) != names)
            throw ApiError<409>{"source_parameters_changed",
                "Параметры ключа изменены. Используйте новый ключ: " + key};
        json::ValueBuilder row;
        row["key"] = key;
        row["ru"] = text;
        row["en"] = english->second;
        row["parameters"] = parameters;
        rows.PushBack(row.ExtractValue());
    }
    auto values = rows.ExtractValue();
    if (json::ToString(values).size() > 2 * 1024 * 1024)
        throw ApiError<400>{"catalog_too_large", "Каталог превышает 2 МБ"};
    tx.Execute(sql::kRegisterSource, values);
    tx.Commit();
    return Response200{.registered = static_cast<int>(ru.size())};
}
}  // namespace localization::views::api::internal::localization::sources::upsert
