#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace transport_locations::helpers {
// CSV допускает запятые, переносы строк и удвоенные кавычки внутри поля.
inline std::vector<std::string> ReadCsvRow(std::string_view csv, std::size_t& offset) {
    std::vector<std::string> fields;
    do {
        std::string field;
        if (offset < csv.size() && csv[offset] == '"') {
            ++offset;
            bool closed = false;
            while (offset < csv.size()) {
                const auto ch = csv[offset++];
                if (ch == '"') {
                    if (offset < csv.size() && csv[offset] == '"')
                        ++offset;
                    else {
                        closed = true;
                        break;
                    }
                }
                field += ch;
                if (field.size() > 16384)
                    throw std::runtime_error("Слишком длинное поле справочника");
            }
            if (!closed)
                throw std::runtime_error("Незакрытая кавычка CSV справочника");
        } else {
            while (offset < csv.size() && csv[offset] != ',' && csv[offset] != '\r' &&
                csv[offset] != '\n') {
                if (csv[offset] == '"' || field.size() >= 16384)
                    throw std::runtime_error("Некорректное поле CSV справочника");
                field += csv[offset++];
            }
        }
        fields.push_back(std::move(field));
        if (fields.size() > 100)
            throw std::runtime_error("Слишком много колонок CSV справочника");
        if (offset == csv.size())
            break;
        const auto delimiter = csv[offset++];
        if (delimiter == '\n')
            break;
        if (delimiter == '\r') {
            if (offset < csv.size() && csv[offset] == '\n')
                ++offset;
            break;
        }
        if (delimiter != ',')
            throw std::runtime_error("Некорректный разделитель CSV справочника");
    } while (offset <= csv.size());
    return fields;
}

}  // namespace transport_locations::helpers
