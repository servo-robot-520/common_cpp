//
// Created by greenhand520 on 2026/6/29.
//

#pragma once

#include <format>
#include <fstream>
#include <sstream>
#include <string>

#define CLASS_NAME(T) std::string_view(#T).substr(std::string_view(#T).rfind(':') + 1)

#define FOR_ENUM(x) x
#define FOR_NAME(x) #x

#define ENUM_TO_STRING_CASE(EnumValue) \
    case EnumValue:                    \
        return #EnumValue

namespace common_cpp::common_utils {

    /// Check whether the type has a size() method (used to distinguish native arrays and containers)
    template <typename T, typename = void>
    struct has_size : std::false_type {
    };

    template <typename T>
    struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
        : std::true_type {
    };

    /// Convert container to string
    template <typename Container>
    std::string container_to_str(const Container& values) {
        std::ostringstream oss;

        // 依赖编译来检查
        for (bool first = true; const auto& element : values) {
            if (!first) {
                oss << ",";
            }
            if constexpr (std::is_same_v<std::remove_cv_t<typename Container::value_type>, char> ||
                          std::is_same_v<std::remove_cv_t<typename Container::value_type>, unsigned char> ||
                          std::is_same_v<std::remove_cv_t<typename Container::value_type>, signed char>) {
                oss << std::format("{}", static_cast<int>(element));
            }
            else {
                oss << std::format("{}", element);
            }
            first = false;
        }
        return oss.str();
    }

    /// Convert array to string, specialized version of native array (requires additional input length)
    template <typename T>
    std::string array_to_str(const T arr[], const int length) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);

        for (int i = 0; i < length; ++i) {
            if (i > 0)
                oss << ", ";
            oss << arr[i];
        }

        return oss.str();
    }

    std::string to_lower_case(std::string str);

    long get_current_sys_nanoseconds();

    long get_current_sys_millis();

    long get_app_started_millis();

    std::string simple_uuid();

    std::string generate_uuid();

    [[nodiscard]] bool is_ascii_whitespace(char character) noexcept;

    [[nodiscard]] bool is_empty_or_ascii_whitespace(std::string_view value) noexcept;


} // namespace common_utils
