#pragma once
#include <regex>
#include <set>
#include <string>

namespace localization::helpers {
inline std::set<std::string> Parameters(const std::string& text) {
    static const std::regex pattern{R"(\{\w+\})"};
    std::set<std::string> result;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator(); ++it)
        result.insert(it->str());
    return result;
}

}  // namespace localization::helpers
