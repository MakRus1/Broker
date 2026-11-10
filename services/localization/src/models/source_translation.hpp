#pragma once
#include <string>
#include <vector>

namespace localization {
struct SourceTranslation {
    std::string Key;
    std::string Ru;
    std::string En;
    std::vector<std::string> Parameters;
};
}  // namespace localization
