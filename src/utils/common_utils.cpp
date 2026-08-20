//
// Created by greenhand520 on 2026/6/29.
//

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "common_cpp/utils/common_utils.hpp"


namespace fs = std::filesystem;

namespace common_cpp {

    std::string common_utils::to_lower_case(std::string str) {
        for (auto& c : str) {
            c = std::tolower(c);
        }
        return str;
    }

    long common_utils::get_current_sys_nanoseconds() {
        const auto now = std::chrono::system_clock::now();
        const auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    long common_utils::get_current_sys_millis() {
        const auto now = std::chrono::system_clock::now();
        const auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    long common_utils::get_app_started_millis() {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    std::string common_utils::simple_uuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;

        for (int i = 0; i < 32; i++) {
            // 在标准 UUID 位置插入版本号和变体号
            if (i == 12) {
                ss << "4"; // UUID version 4
            }
            else if (i == 16) {
                ss << std::hex << dis2(gen); // variant
            }
            else {
                ss << std::hex << dis(gen);
            }
        }

        return ss.str();
    }

    bool common_utils::is_ascii_whitespace(const char character) noexcept {
        return character == ' ' || (character >= '\t' && character <= '\r');
    }

    bool common_utils::is_empty_or_ascii_whitespace(const std::string_view value) noexcept {
        if (value.empty()) {
            return true;
        }

        return std::ranges::all_of(value.begin(), value.end(), is_ascii_whitespace);
    }

    std::string common_utils::generate_uuid() {
        return simple_uuid();
    }

} // namespace common_cpp
